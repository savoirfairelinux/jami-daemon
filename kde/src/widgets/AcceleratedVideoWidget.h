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
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 **************************************************************************/
#ifndef ACCELERATEDVIDEOWIDGET_H
#define ACCELERATEDVIDEOWIDGET_H
#include <QGLPixelBuffer>
#include <QGLWidget>
#include <QPixmap>

 #include <QtOpenGL/qgl.h>
 #include <QtCore/qvector.h>
 #include <QtGui/qmatrix4x4.h>
 #include <QtGui/qvector3d.h>
 #include <QtGui/qvector2d.h>


class Geometry;
//class Cube;
class Tile;



class QPropertyAnimation;

class Geometry
{
public:
   void loadArrays() const;
   void addQuad(const QVector3D &a, const QVector3D &b,
               const QVector3D &c, const QVector3D &d,
               const QVector<QVector2D> &tex);
   void setColors(int start, GLfloat colors[4][4]);
   const GLushort *indices() const { return faces.constData(); }
   int count() const { return faces.count(); }
private:
   QVector<GLushort> faces;
   QVector<QVector3D> vertices;
   QVector<QVector3D> normals;
   QVector<QVector2D> texCoords;
   QVector<QVector4D> colors;
   int append(const QVector3D &a, const QVector3D &n, const QVector2D &t);
   void addTri(const QVector3D &a, const QVector3D &b, const QVector3D &c, const QVector3D &n);
   friend class Tile;
};

class Tile
{
public:
   void draw() const;
   void setColors(GLfloat[4][4]);
protected:
   Tile(const QVector3D &loc = QVector3D());
   QVector3D location;
   QQuaternion orientation;
private:
   int start;
   int count;
   bool useFlatColor;
   GLfloat faceColor[4];
   Geometry *geom;
   friend class TileBuilder;
};

class TileBuilder
{
public:
   enum { bl, br, tr, tl };
   explicit TileBuilder(Geometry *, qreal depth = 0.0f, qreal size = 1.0f);
   Tile *newTile(const QVector3D &loc = QVector3D()) const;
   void setColor(QColor c) { color = c; }
protected:
   void initialize(Tile *) const;
   QVector<QVector3D> verts;
   QVector<QVector2D> tex;
   int start;
   int count;
   Geometry *geom;
   QColor color;
};

class Cube : public QObject, public Tile
{
   Q_OBJECT
   Q_PROPERTY(qreal range READ range WRITE setRange)
   Q_PROPERTY(qreal altitude READ altitude WRITE setAltitude)
   Q_PROPERTY(qreal rotation READ rotation WRITE setRotation)
public:
   explicit Cube(const QVector3D &loc = QVector3D());
   ~Cube();
   qreal range() { return location.x(); }
   void setRange(qreal r);
   qreal altitude() { return location.y(); }
   void setAltitude(qreal a);
   qreal rotation() { return rot; }
   void setRotation(qreal r);
   void removeBounce();
   void startAnimation();
   void setAnimationPaused(bool paused);
signals:
   void changed();
private:
   qreal rot;
   QPropertyAnimation *r;
   QPropertyAnimation *a;
   QPropertyAnimation *rtn;
   qreal startx;
   friend class CubeBuilder;
};

class CubeBuilder : public TileBuilder
{
public:
   explicit CubeBuilder(Geometry *, qreal depth = 0.0f, qreal size = 1.0f);
   Cube *newCube(const QVector3D &loc = QVector3D()) const;
private:
   mutable int ix;
};






///Hardware accelerated version of VideoWidget
class AcceleratedVideoWidget : public QGLWidget
{
   Q_OBJECT
public:
   explicit AcceleratedVideoWidget(QWidget* parent);
   ~AcceleratedVideoWidget();

private:
   QGLPixelBuffer* m_pPixBuf;
   QImage m_Image;

   qreal aspect;
   GLuint dynamicTexture;
   GLuint cubeTexture;
   bool hasDynamicTextureUpdate;
   QGLPixelBuffer *pbuffer;
   Geometry *geom;
   Cube *cube;
   Tile *backdrop;
   QList<Cube *> cubes;
   QList<Tile *> tiles;
   
   void initializeGeometry();
   void initPbuffer();
   void initCommon();
   void perspectiveProjection();
   void orthographicProjection();
   void drawPbuffer();
   void setAnimationPaused(bool enable);
     
protected:
   void initializeGL();
   void resizeGL(int w, int h);
   void paintGL();
   void mousePressEvent(QMouseEvent *) { setAnimationPaused(true); }
   void mouseReleaseEvent(QMouseEvent *) { setAnimationPaused(false); }

private slots:
   void newFrameEvent();
};
#endif
