from PIL import Image, ImageDraw
import cv2
from collections import Counter
import heapq
import sys
import os
# import subprocess

MODE_RECTANGLE = 1
MODE_ELLIPSE = 2
MODE_ROUNDED_RECTANGLE = 3

MODE = MODE_RECTANGLE
ITERATIONS = 1024 * 12
LEAF_SIZE = 4
PADDING = 0
FILL_COLOR = (0, 0, 0)
SAVE_FRAMES = False
ERROR_RATE = 0.5
AREA_POWER = 0.25
OUTPUT_SCALE = 1


def weighted_average(hist):
    total = sum(hist)
    value = sum(i * x for i, x in enumerate(hist)) / total
    error = sum(x * (value - i) ** 2 for i, x in enumerate(hist)) / total
    error = error**0.5
    return value, error


def color_from_histogram(hist):
    r, re = weighted_average(hist[:256])
    g, ge = weighted_average(hist[256:512])
    b, be = weighted_average(hist[512:768])
    e = re * 0.2989 + ge * 0.5870 + be * 0.1140
    return (int(r), int(g), int(b)), e


def rounded_rectangle(draw, box, radius, color):
    left, top, right, bottom = box
    d = min(radius * 2, right - left, bottom - top)
    radius = d / 2
    draw.ellipse((left, top, left + d, top + d), fill=color)
    draw.ellipse((right - d, top, right, top + d), fill=color)
    draw.ellipse((left, bottom - d, left + d, bottom), fill=color)
    draw.ellipse((right - d, bottom - d, right, bottom), fill=color)
    draw.rectangle((left, top + radius, right, bottom - radius), fill=color)
    draw.rectangle((left + radius, top, right - radius, bottom), fill=color)


class Quad(object):
    def __init__(self, model, box, depth):
        self.model = model
        self.box = box
        self.depth = depth
        hist = self.model.im.crop(self.box).histogram()
        self.color, self.error = color_from_histogram(hist)
        self.leaf = self.is_leaf()
        self.area = self.compute_area()
        self.children = []

    def is_leaf(self):
        left, top, right, bottom = self.box
        return int(right - left <= LEAF_SIZE or bottom - top <= LEAF_SIZE)

    def compute_area(self):
        left, top, right, bottom = self.box
        return (right - left) * (bottom - top)

    def split(self):
        left, top, right, bottom = self.box
        lr = left + (right - left) / 2
        tb = top + (bottom - top) / 2
        depth = self.depth + 1
        tl = Quad(self.model, (left, top, lr, tb), depth)
        tr = Quad(self.model, (lr, top, right, tb), depth)
        bl = Quad(self.model, (left, tb, lr, bottom), depth)
        br = Quad(self.model, (lr, tb, right, bottom), depth)
        self.children = (tl, tr, bl, br)
        return self.children

    def get_leaf_nodes(self, max_depth=None):
        if not self.children:
            return [self]
        if max_depth is not None and self.depth >= max_depth:
            return [self]
        result = []
        for child in self.children:
            result.extend(child.get_leaf_nodes(max_depth))
        return result


class Model(object):
    def __init__(self, path):
        self.im = Image.open(path).convert("RGB")
        self.width, self.height = self.im.size
        self.heap = []
        self.root = Quad(self, (0, 0, self.width, self.height), 0)
        self.error_sum = self.root.error * self.root.area
        self.push(self.root)

    @property
    def quads(self):
        return [x[-1] for x in self.heap]

    def average_error(self):
        return self.error_sum / (self.width * self.height)

    def push(self, quad):
        score = -quad.error * (quad.area**AREA_POWER)
        heapq.heappush(self.heap, (quad.leaf, score, quad))

    def pop(self):
        return heapq.heappop(self.heap)[-1]

    def split(self):
        quad = self.pop()
        self.error_sum -= quad.error * quad.area
        children = quad.split()
        for child in children:
            self.push(child)
            self.error_sum += child.error * child.area

    def render(self, path, max_depth=None, frame_number=None):
        m = OUTPUT_SCALE
        dx, dy = (PADDING, PADDING)
        im = Image.new("RGB", (self.width * m + dx, self.height * m + dy))
        draw = ImageDraw.Draw(im)
        draw.rectangle((0, 0, self.width * m, self.height * m), fill=FILL_COLOR)
        for quad in self.root.get_leaf_nodes(max_depth):
            left, top, right, bottom = quad.box
            box = (left * m + dx, top * m + dy, right * m - 1, bottom * m - 1)
            if MODE == MODE_ELLIPSE:
                draw.ellipse(box, fill=quad.color)
            elif MODE == MODE_ROUNDED_RECTANGLE:
                radius = m * min((right - left), (bottom - top)) / 4
                rounded_rectangle(draw, box, radius, quad.color)
            else:
                draw.rectangle(box, fill=quad.color)
        del draw

        if frame_number is not None:
            im.save(path % frame_number, "PNG")  # Save frame with specified filename
        else:
            im.save(path, "PNG")  # Save final output image


def create_video_from_frames(frames_dir, output_video, fps=4):
    # Get a list of all files in the directory
    files = os.listdir(frames_dir)

    # Filter out non-image files (you can add more extensions if needed)
    image_extensions = ['.jpg', '.jpeg', '.png', '.bmp', '.tiff']
    image_files = [f for f in files if os.path.splitext(f)[1].lower() in image_extensions]
    image_files.sort()

    image = cv2.imread(os.path.join(frames_dir, image_files[0]))
    height, width = image.shape[:2]
    writer = cv2.VideoWriter(output_video, cv2.VideoWriter_fourcc(*"mp4v"), fps, (width, height))

    # Iterate over image files and read them with OpenCV
    for image_file in image_files:
        # Construct the full path to the image file
        image_path = os.path.join(frames_dir, image_file)
    
        # Read the image using OpenCV
        image = cv2.imread(image_path)

        # Check if the image was read successfully
        if image is not None:
            # resiz = cv2.resize(image, (width, height))
            writer.write(image)
        else:
            print(f'Failed to read image: {image_file}')

    writer.release()


def main():
    args = sys.argv[1:]
    if len(args) != 1:
        print("Usage: python main.py input_image")
        return
    model = Model(args[0])
    previous = None

    frames_path = "frames/%06d.png"  # Path pattern for saving frames
    for i in range(ITERATIONS):
        error = model.average_error()
        if previous is None or previous - error > ERROR_RATE:
            print(i, error)
            model.render(frames_path, frame_number=i)  # Save frames during iteration
            previous = error
        model.split()

    model.render("output.png")  # Save final output image
    print("-" * 32)
    depth = Counter(x.depth for x in model.quads)
    for key in sorted(depth):
        value = depth[key]
        n = 4**key
        pct = 100.0 * value / n
        print("%3d %8d %8d %8.2f%%" % (key, n, value, pct))
    print("-" * 32)
    print("             %8d %8.2f%%" % (len(model.quads), 100))

    # Generate GIF from saved frames
    frames_dir = "frames"

    # Create a video from the saved frames
    create_video_from_frames(frames_dir, "output_video.mp4")

    # subprocess.run(['./video_player', 'output_video.mp4'])


if __name__ == "__main__":
    main()

    print("Success")
