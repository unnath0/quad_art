#include "tinyfiledialogs/tinyfiledialogs.h"
#include <GL/freeglut_std.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// window dimensions
int win_width = 800;
int win_height = 600;

// function to display text
void renderBitmapString(float x, float y, void *font, const char *string) {
  const char *c;
  glRasterPos2f(x, y);

  for (c = string; *c != '\0'; c++) {
    glutBitmapCharacter(font, *c);
  }
}

void mouse(int btn, int state, int x, int y) {
  if (btn == GLUT_LEFT_BUTTON && state == GLUT_DOWN) {
    // convert GLUT coordinates to OpenGL coordinates
    float fx = (float)x / (win_width / 2) - 1.0f;
    float fy = 1.0f - (float)y / (win_height / 2);

    // check if this position is within the image upload boundary
    if (fx >= -0.2f && fx <= 0.2f && fy >= -0.05f && fy <= 0.05f) {
      // open file dialog
      const char *filter_patterns[3] = {"*.jpeg", "*.jpg", "*.png"};
      const char *file_path = tinyfd_openFileDialog(
        "Select an image", 
        "", 
        3, 
        filter_patterns,
        NULL, 
        0
      );

      if (file_path) {
        printf("Selected file is %s\n", file_path);

        // prepare the command to call the python script
        char command[512];
        snprintf(command, sizeof(command), "python quad_art_generate.py \"%s\"", file_path);

        // call the python script
        int result = system(command);
        if ( result == -1 ) {
          fprintf(stderr, "Failed to execute python script");
        }
      }
    }
  }
}

void display(void) {
  glClear(GL_COLOR_BUFFER_BIT);

  glColor3f(0.0, 0.0, 0.0);

  // display project title
  renderBitmapString(-0.1, 0.8, GLUT_BITMAP_HELVETICA_18, "Quad Art");

  // display team members
  renderBitmapString(-0.1, 0.6, GLUT_BITMAP_HELVETICA_12, "Team Members");
  renderBitmapString(-0.1, 0.5, GLUT_BITMAP_HELVETICA_12, "Abhijnan S");
  renderBitmapString(-0.1, 0.4, GLUT_BITMAP_HELVETICA_12, "Anand S P");

  // image upload button
  renderBitmapString(-0.1, 0.0, GLUT_BITMAP_HELVETICA_18, "Upload an image");

  glFlush();
}

void init(void) {
  glClearColor(1.0, 1.0, 1.0, 1.0);
  // pass
}

int main(int argc, char *argv[]) {
  glutInit(&argc, argv);
  glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);
  glutInitWindowSize(win_width, win_height);
  glutCreateWindow("Quad Art");

  init();

  glutDisplayFunc(display);
  glutMouseFunc(mouse);
  glutMainLoop();

  return 0;
}
