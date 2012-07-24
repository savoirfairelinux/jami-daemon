/***************************************************************************
 *   Copyright (C) 2009-2012 by Savoir-Faire Linux                         *
 *   Author : Emmanuel Lepage Valle <emmanuel.lepage@savoirfairelinux.com >*
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 **************************************************************************/
#include "AcceleratedVideoWidget.h"
#include <QPixmap>
#include <KDebug>
#include "../lib/VideoModel.h"
#include "../lib/VideoRenderer.h"



#include <QtGui/QImage>
 #include <QtCore/QPropertyAnimation>

static const qreal FACE_SIZE = 0.4;

static const qreal speeds[] = { 1.8f, 2.4f, 3.6f };
static const qreal amplitudes[] = { 2.0f, 2.5f, 3.0f };

static inline void qSetColor(float colorVec[], QColor c)
{
   colorVec[0] = c.redF();
   colorVec[1] = c.greenF();
   colorVec[2] = c.blueF();
   colorVec[3] = c.alphaF();
}

int Geometry::append(const QVector3D &a, const QVector3D &n, const QVector2D &t)
{
   int v = vertices.count();
   vertices.append(a);
   normals.append(n);
   texCoords.append(t);
   faces.append(v);
   colors.append(QVector4D(0.6f, 0.6f, 0.6f, 1.0f));
   return v;
}

void Geometry::addQuad(const QVector3D &a, const QVector3D &b,
                           const QVector3D &c, const QVector3D &d,
                           const QVector<QVector2D> &tex)
{
   QVector3D norm = QVector3D::normal(a, b, c);
   // append first triangle
   int aref = append(a, norm, tex[0]);
   append(b, norm, tex[1]);
   int cref = append(c, norm, tex[2]);
   // append second triangle
   faces.append(aref);
   faces.append(cref);
   append(d, norm, tex[3]);
}

void Geometry::loadArrays() const
{
   glEnableClientState(GL_VERTEX_ARRAY);
   glEnableClientState(GL_NORMAL_ARRAY);
   glEnableClientState(GL_TEXTURE_COORD_ARRAY);
   glEnableClientState(GL_COLOR_ARRAY);
   glVertexPointer(3, GL_FLOAT, 0, vertices.constData());
   glNormalPointer(GL_FLOAT, 0, normals.constData());
   glTexCoordPointer(2, GL_FLOAT, 0, texCoords.constData());
   glColorPointer(4, GL_FLOAT, 0, colors.constData());
}

void Geometry::setColors(int start, GLfloat colorArray[4][4])
{
   int off = faces[start];
   for (int i = 0; i < 4; ++i)
      colors[i + off] = QVector4D(colorArray[i][0],
                                    colorArray[i][1],
                                    colorArray[i][2],
                                    colorArray[i][3]);
}

Tile::Tile(const QVector3D &loc)
   : location(loc)
   , start(0)
   , count(0)
   , useFlatColor(false)
   , geom(0)
{
   qSetColor(faceColor, QColor(Qt::darkGray));
}

void Tile::setColors(GLfloat colorArray[4][4])
{
   useFlatColor = true;
   geom->setColors(start, colorArray);
}

static inline void qMultMatrix(const QMatrix4x4 &mat)
{
   if (sizeof(qreal) == sizeof(GLfloat))
      glMultMatrixf((GLfloat*)mat.constData());
#ifndef QT_OPENGL_ES
   else if (sizeof(qreal) == sizeof(GLdouble))
      glMultMatrixd((GLdouble*)mat.constData());
#endif
   else
   {
      GLfloat fmat[16];
      qreal const *r = mat.constData();
      for (int i = 0; i < 16; ++i)
            fmat[i] = r[i];
      glMultMatrixf(fmat);
   }
}

void Tile::draw() const
{
   QMatrix4x4 mat;
   mat.translate(location);
   mat.rotate(orientation);
   glMatrixMode(GL_MODELVIEW);
   glPushMatrix();
   qMultMatrix(mat);
   glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE, faceColor);
   glDrawElements(GL_TRIANGLES, count, GL_UNSIGNED_SHORT, geom->indices() + start);
   glPopMatrix();
}

TileBuilder::TileBuilder(Geometry *g, qreal depth, qreal size)
   : verts(4)
   , tex(4)
   , start(g->count())
   , count(0)
   , geom(g)
{
   // front face - make a square with bottom-left at origin
   verts[br].setX(size);
   verts[tr].setX(size);
   verts[tr].setY(size);
   verts[tl].setY(size);

   // these vert numbers are good for the tex-coords
   for (int i = 0; i < 4; ++i)
      tex[i] = verts[i].toVector2D();

   // now move verts half cube width across so cube is centered on origin
   for (int i = 0; i < 4; ++i)
      verts[i] -= QVector3D(size / 2.0f, size / 2.0f, -depth);

   // add the front face
   g->addQuad(verts[bl], verts[br], verts[tr], verts[tl], tex);

   count = g->count() - start;
}

void TileBuilder::initialize(Tile *tile) const
{
   tile->start = start;
   tile->count = count;
   tile->geom = geom;
   qSetColor(tile->faceColor, color);
}

Tile *TileBuilder::newTile(const QVector3D &loc) const
{
   Tile *tile = new Tile(loc);
   initialize(tile);
   return tile;
}

Cube::Cube(const QVector3D &loc)
   : Tile(loc)
   , rot(0.0f)
   , r(0), a(0)
{
}

Cube::~Cube()
{
}

void Cube::setAltitude(qreal a)
{
   if (location.y() != a)
   {
      location.setY(a);
      emit changed();
   }
}

void Cube::setRange(qreal r)
{
   if (location.x() != r)
   {
      location.setX(r);
      emit changed();
   }
}

void Cube::setRotation(qreal r)
{
   if (r != rot)
   {
      orientation = QQuaternion::fromAxisAndAngle(QVector3D(1.0f, 1.0f, 1.0f), r);
      emit changed();
   }
}

void Cube::removeBounce()
{
   delete a;
   a = 0;
   delete r;
   r = 0;
}

void Cube::startAnimation()
{
   if (r)
   {
      r->start();
      r->setCurrentTime(startx);
   }
   if (a)
      a->start();
   if (rtn)
      rtn->start();
}

void Cube::setAnimationPaused(bool paused)
{
   if (paused)
   {
      if (r)
            r->pause();
      if (a)
            a->pause();
      if (rtn)
            rtn->pause();
   }
   else
   {
      if (r)
            r->resume();
      if (a)
            a->resume();
      if (rtn)
            rtn->resume();
   }
}

CubeBuilder::CubeBuilder(Geometry *g, qreal depth, qreal size)
   : TileBuilder(g, depth)
   , ix(0)
{
   for (int i = 0; i < 4; ++i)
      verts[i].setZ(size / 2.0f);
   // back face - "extrude" verts down
   QVector<QVector3D> back(verts);
   for (int i = 0; i < 4; ++i)
      back[i].setZ(-size / 2.0f);

   // add the back face
   g->addQuad(back[br], back[bl], back[tl], back[tr], tex);

   // add the sides
   g->addQuad(back[bl], back[br], verts[br], verts[bl], tex);
   g->addQuad(back[br], back[tr], verts[tr], verts[br], tex);
   g->addQuad(back[tr], back[tl], verts[tl], verts[tr], tex);
   g->addQuad(back[tl], back[bl], verts[bl], verts[tl], tex);

   count = g->count() - start;
}

Cube *CubeBuilder::newCube(const QVector3D &loc) const
{
   Cube *c = new Cube(loc);
   initialize(c);
   qreal d = 4000.0f;
   qreal d3 = d / 3.0f;
   // Animate movement from left to right
   c->r = new QPropertyAnimation(c, "range");
   c->r->setStartValue(-1.3f);
   c->r->setEndValue(1.3f);
   c->startx = ix * d3 * 3.0f;
   c->r->setDuration(d * 4.0f);
   c->r->setLoopCount(-1);
   c->r->setEasingCurve(QEasingCurve(QEasingCurve::CosineCurve));
   // Animate movement from bottom to top
   c->a = new QPropertyAnimation(c, "altitude");
   c->a->setEndValue(loc.y());
   c->a->setStartValue(loc.y() + amplitudes[ix]);
   c->a->setDuration(d / speeds[ix]);
   c->a->setLoopCount(-1);
   c->a->setEasingCurve(QEasingCurve(QEasingCurve::CosineCurve));
   // Animate rotation
   c->rtn = new QPropertyAnimation(c, "rotation");
   c->rtn->setStartValue(c->rot);
   c->rtn->setEndValue(359.0f);
   c->rtn->setDuration(d * 2.0f);
   c->rtn->setLoopCount(-1);
   c->rtn->setDuration(d / 2);
   ix = (ix + 1) % 3;
   return c;
}


void AcceleratedVideoWidget::newFrameEvent()
{
   qDebug() << "New frame event";
   QSize size(VideoModel::getInstance()->getRenderer()->getActiveResolution().width, VideoModel::getInstance()->getRenderer()->getActiveResolution().height);
   m_Image = QImage((uchar*)VideoModel::getInstance()->getRenderer()->rawData() , size.width(), size.height(), QImage::Format_ARGB32 );
   paintGL();
}

static GLfloat colorArray[][4] = {
     {0.243f , 0.423f , 0.125f , 1.0f},
     {0.176f , 0.31f  , 0.09f  , 1.0f},
     {0.4f   , 0.69f  , 0.212f , 1.0f},
     {0.317f , 0.553f , 0.161f , 1.0f}
 };

 AcceleratedVideoWidget::AcceleratedVideoWidget(QWidget *parent)
     : QGLWidget(QGLFormat(QGL::SampleBuffers), parent)
     , geom(0)
     , cube(0)
 {
     // create the pbuffer
     QGLFormat pbufferFormat = format();
     pbufferFormat.setSampleBuffers(false);
     pbuffer = new QGLPixelBuffer(QSize(512, 512), pbufferFormat, this);
     setWindowTitle(tr("OpenGL pbuffers"));
     initializeGeometry();
     connect(VideoModel::getInstance(),SIGNAL(frameUpdated()),this,SLOT(newFrameEvent()));
 }

 AcceleratedVideoWidget::~AcceleratedVideoWidget()
 {
     pbuffer->releaseFromDynamicTexture();
     glDeleteTextures(1, &dynamicTexture);
     delete pbuffer;

     qDeleteAll(cubes);
     qDeleteAll(tiles);
     delete cube;
 }

 void AcceleratedVideoWidget::initializeGL()
 {
     initCommon();
     glShadeModel(GL_SMOOTH);
     glEnable(GL_LIGHTING);
     glEnable(GL_LIGHT0);
     static GLfloat lightPosition[4] = { 0.5, 5.0, 7.0, 1.0 };
     glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);
     initPbuffer();
     cube->startAnimation();
     connect(cube, SIGNAL(changed()), this, SLOT(update()));
     for (int i = 0; i < 3; ++i)
     {
         cubes[i]->startAnimation();
         connect(cubes[i], SIGNAL(changed()), this, SLOT(update()));
     }
 }

 void AcceleratedVideoWidget::paintGL()
 {
   QSize size(VideoModel::getInstance()->getRenderer()->getActiveResolution().width, VideoModel::getInstance()->getRenderer()->getActiveResolution().height);
   if (size != minimumSize())
      setMinimumSize(size);
   
   pbuffer->makeCurrent();
   drawPbuffer();
   // On direct render platforms, drawing onto the pbuffer context above
   // automatically updates the dynamic texture.  For cases where rendering
   // directly to a texture is not supported, explicitly copy.
   if (!hasDynamicTextureUpdate)
      pbuffer->updateDynamicTexture(dynamicTexture);
   makeCurrent();

   // Use the pbuffer as a texture to render the scene
   glBindTexture(GL_TEXTURE_2D, dynamicTexture);

   // set up to render the scene
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
   glLoadIdentity();
   glTranslatef(0.0f, 0.0f, -10.0f);

   // draw the background
   glPushMatrix();
   glScalef(aspect, 1.0f, 1.0f);
   for (int i = 0; i < tiles.count(); ++i)
      tiles[i]->draw();
   glPopMatrix();

   // draw the bouncing cubes
   for (int i = 0; i < cubes.count(); ++i)
      cubes[i]->draw();
 }

 void AcceleratedVideoWidget::initializeGeometry()
 {
     geom = new Geometry();
     CubeBuilder cBuilder(geom, 0.5);
     cBuilder.setColor(QColor(255, 255, 255, 212));
     // build the 3 bouncing, spinning cubes
     for (int i = 0; i < 3; ++i)
         cubes.append(cBuilder.newCube(QVector3D((float)(i-1), -1.5f, 5 - i)));

     // build the spinning cube which goes in the dynamic texture
     cube = cBuilder.newCube();
     cube->removeBounce();

     // build the background tiles
     TileBuilder tBuilder(geom);
     tBuilder.setColor(QColor(Qt::white));
     for (int c = -2; c <= +2; ++c)
         for (int r = -2; r <= +2; ++r)
             tiles.append(tBuilder.newTile(QVector3D(c, r, 0)));

     // graded backdrop for the pbuffer scene
     TileBuilder bBuilder(geom, 0.0f, 2.0f);
     bBuilder.setColor(QColor(102, 176, 54, 210));
     backdrop = bBuilder.newTile(QVector3D(0.0f, 0.0f, -1.5f));
     backdrop->setColors(colorArray);
 }

 void AcceleratedVideoWidget::initCommon()
 {
     qglClearColor(QColor(Qt::darkGray));

     glEnable(GL_DEPTH_TEST);
     glEnable(GL_CULL_FACE);
     glEnable(GL_MULTISAMPLE);

     glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
     glEnable(GL_BLEND);

     glEnable(GL_TEXTURE_2D);

     geom->loadArrays();
 }

 void AcceleratedVideoWidget::perspectiveProjection()
 {
     glMatrixMode(GL_PROJECTION);
     glLoadIdentity();
 #ifdef QT_OPENGL_ES
     glFrustumf(-aspect, +aspect, -1.0, +1.0, 4.0, 15.0);
 #else
     glFrustum(-aspect, +aspect, -1.0, +1.0, 4.0, 15.0);
 #endif
     glMatrixMode(GL_MODELVIEW);
 }

 void AcceleratedVideoWidget::orthographicProjection()
 {
     glMatrixMode(GL_PROJECTION);
     glLoadIdentity();
 #ifdef QT_OPENGL_ES
     glOrthof(-1.0, +1.0, -1.0, +1.0, -90.0, +90.0);
 #else
     glOrtho(-1.0, +1.0, -1.0, +1.0, -90.0, +90.0);
 #endif
     glMatrixMode(GL_MODELVIEW);
 }

 void AcceleratedVideoWidget::resizeGL(int width, int height)
 {
     glViewport(0, 0, width, height);
     aspect = (qreal)width / (qreal)(height ? height : 1);
     perspectiveProjection();
 }

void AcceleratedVideoWidget::drawPbuffer()
{
   cubeTexture = bindTexture(m_Image);
   //initPbuffer();
    
   orthographicProjection();

   glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

   glDisable(GL_TEXTURE_2D);
   backdrop->draw();
   glEnable(GL_TEXTURE_2D);

   glBindTexture(GL_TEXTURE_2D, cubeTexture);
   glDisable(GL_CULL_FACE);
   cube->draw();
   glEnable(GL_CULL_FACE);

   glFlush();
}

 void AcceleratedVideoWidget::initPbuffer()
 {
     pbuffer->makeCurrent();

//      cubeTexture = bindTexture(QImage("/home/lepagee/ccu_12.png"));
     cubeTexture = bindTexture(m_Image);

     initCommon();

     // generate a texture that has the same size/format as the pbuffer
     dynamicTexture = pbuffer->generateDynamicTexture();

     // bind the dynamic texture to the pbuffer - this is a no-op under X11
     hasDynamicTextureUpdate = pbuffer->bindToDynamicTexture(dynamicTexture);
     makeCurrent();
 }

 void AcceleratedVideoWidget::setAnimationPaused(bool enable)
 {
     cube->setAnimationPaused(enable);
     for (int i = 0; i < 3; ++i)
         cubes[i]->setAnimationPaused(enable);
 }
