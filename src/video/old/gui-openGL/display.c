/*
 *  Copyright (C) 2006-2007 Savoir-Faire Linux inc. 
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com
 *  
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */



#include <stdio.h>
#include <stdlib.h>
#include <GL/gl.h>
#include <GL/glut.h>
#include <jpeglib.h>
#include <jerror.h>

#include "../v4l2/capture_v4l2.c"

unsigned char* pix;
camera_v4l2* camera;
// video resolution
int RES_X;
int RES_Y; 
// window size
int WIDTH = 512;
int HEIGHT = 512;
// texture size
int x_t;
int y_t;


/*
 * Sets up appropriate viewport coordinates for 2D
 */
void reshape(int w, int h)
{
	glViewport(0, 0, w, h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, w, h, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void makeQuad(){

	// centrer la video dans la fenetre
	int x1,y1,x2,y2,x3,y3,x4,y4;
	x1 = (WIDTH - x_t) / 2;
	y1 = (HEIGHT - y_t) / 2;
	x2 = x_t + x1;
	y2 = y1;
	x3 = x2;
	y3 = y1 + y_t;
	x4 = x1;	
	y4 = y3;

	glBegin(GL_QUADS);

	glTexCoord3i(0,0,0);
        glVertex3i(x1,y1,0);
        glTexCoord3i(1,0,0);
        glVertex3i(x2,y2,0);
        glTexCoord3i(1,1,0);
        glVertex3i(x3,y3,0);
        glTexCoord3i(0,1,0);
        glVertex3i(x4,y4,0);




	glEnd();
}


/*
 *  display callback function
 */
void display(void){
	
	pix = v4l2_grab_data(camera);
	
	glClear(GL_COLOR_BUFFER_BIT);
	makeQuad();
	glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,RES_X,RES_Y,0,GL_RGB,GL_UNSIGNED_BYTE,pix);
	glutSwapBuffers();

}

void keyboard(unsigned char c, int x, int y){
	switch(c)
	{
	case 27:
	case 'q':
	case 'Q':
		exit(1);
		break;
	case '1':
		x_t = RES_X;
		y_t = RES_Y;
		break;
	case '2':
		x_t = RES_X*2;
                y_t = RES_Y*2;
                break;
	case '3':
                x_t = RES_X*3;
                y_t = RES_Y*3;
                break;
	}
}	


void idle( void )
{
	glutPostRedisplay();
}


int main (int argc, char **argv) {
	// init webcam
	camera = v4l2_init_camera("/dev/video0");
	// set video resolution
	RES_X = camera->width;	
	RES_Y = camera->height;	
	// set texture resolution
	x_t = RES_X;	
	y_t = RES_Y;

	glutInit (&argc, argv);
	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE) ;
	glutInitWindowSize (WIDTH,HEIGHT);
	glutInitWindowPosition (300,200);
	glutCreateWindow ("SFLphone Video Prototype");
	
	// OpenGL texture paramaters
	glEnable(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	//printf("Texture initialization done...........");
	//glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,WIDTH,HEIGHT,0,GL_RGB,GL_UNSIGNED_BYTE,pix);

	// Callback functions
	glutDisplayFunc (display);	
	glutReshapeFunc(reshape);
	glutIdleFunc(idle);
	glutKeyboardFunc(keyboard);	


	glEnable(GL_DEPTH_TEST);

	// main loop
	glutMainLoop ();
	//return 0;
}
