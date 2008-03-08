/*
    Copyright 2004-2005 Andi Peredri <andi@ukr.net> 
    Copyright 2007-2008 Fela Winkelmolen <fela.kde@gmail.com> 
  
    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.
   
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
   
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "cell.h"

#include <QPainter>
#include <QPixmap>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QTimeLine>

#include <KGlobal>
#include <KIconLoader>
#include <KStandardDirs>
#include <KSvgRenderer>
#include <KDebug>

#include "consts.h"
#include "renderer.h"

Cell::NamesMap Cell::directionNames;
KSvgRenderer Cell::allSvg;

void Cell::initPixmaps()
{
    directionNames[Left]            = "0001";
    directionNames[Down]            = "0010";
    directionNames[Down|Left]       = "0011";
    directionNames[Right]           = "0100";
    directionNames[Right|Left]      = "0101";
    directionNames[Right|Down]      = "0110";
    directionNames[Right|Down|Left] = "0111";
    directionNames[Up]              = "1000";
    directionNames[Up|Left]         = "1001";
    directionNames[Up|Down]         = "1010";
    directionNames[Up|Down|Left]    = "1011";
    directionNames[Up|Right]        = "1100";
    directionNames[Up|Right|Left]   = "1101";
    directionNames[Up|Right|Down]   = "1110";
}

Cell::Cell(QWidget* parent, int i) : QWidget(parent)
{
    angle     = 0;
    light     = 0;
    iindex    = i;
    ddirs     = None;
    connected = false;
    root      = false;
    locked    = false;
    pixmapCache = new QPixmap(width(), height());
    forgroundCache = new QPixmap(width(), height());
    
    forgroundChanged = true;
    setAttribute(Qt::WA_Hover, true);
    
    timeLine = new QTimeLine(AnimationTime, this);
    timeLine->setCurveShape(QTimeLine::EaseOutCurve);
    //timeLine->setUpdateInterval(80);
    connect(timeLine, SIGNAL(frameChanged(int)), SLOT(rotateStep(int)));        
}

Cell::~Cell()
{
    delete pixmapCache;
    delete forgroundCache;
    delete timeLine;
}

int Cell::index() const
{
    return iindex;
}

Directions Cell::dirs() const
{
    return ddirs;
}

bool Cell::isConnected() const
{
    // animating cables are never connected
    //if (timeLine->state() == QTimeLine::Running) return false;
    
    return connected;
}

bool Cell::isRotated() const
{
    return angle;
}

bool Cell::isLocked() const
{
    return locked;
}

void Cell::setLocked( bool newlocked )
{
    if ( locked == newlocked ) return;
    locked = newlocked;
    forgroundChanged = true;
    update();
}


void Cell::setDirs(Directions d)
{
    if(ddirs == d) return;
    ddirs = d;
    forgroundChanged = true;
    update();
}

void Cell::setConnected(bool b)
{
    if(connected == b) return;
    connected = b;
    forgroundChanged = true;
    update();
}

void Cell::setRoot(bool b)
{
    if(root == b) return;
    root = b;
    cableChanged = true;
    if (!(ddirs & None)) 
       forgroundChanged = true;
    update();
}

void Cell::setLight(int l)
{
    light = l;
    forgroundChanged = true;
    update();
}

void Cell::paintEvent(QPaintEvent*)
{
    if (width() == 0 || height() == 0) {
        kDebug() << "Painting empty area";
        return;
    }
    
    if (forgroundChanged) {
        // paint the terminals or server on the forgroundCache
        paintForground();
    }
    
    if (ddirs == None /*|| ddirs == Free*/) {
        *pixmapCache = *forgroundCache;
    } else if (forgroundChanged || cableChanged) {
        // paints everything on the cache
        paintOnCache();
    }
    
    
    QPainter painter;
    painter.begin(this);
    
    // light on hover
    if (underMouse() && !locked) {
        painter.setBrush(HoveredCellColor);
        painter.setPen(Qt::NoPen);
        painter.drawRect(0, 0, pixmapCache->width(), pixmapCache->height());
    }
    
    painter.drawPixmap(0, 0, *pixmapCache);
    painter.end();
    
    forgroundChanged = false;
    cableChanged = false;
}

void Cell::paintForground()
{
    if (root) {
        *forgroundCache = 
                Renderer::self()->computerPixmap(width(), root, isConnected());
    } else if(ddirs == Up || ddirs == Left || ddirs == Down || ddirs == Right) {
        // if the cell has only one direction and isn't a server
        *forgroundCache = 
                Renderer::self()->computerPixmap(width(), root, isConnected());
    } else { 
        forgroundCache->fill(Qt::transparent);
    }
}

void Cell::paintOnCache()
{   
    if (locked) {
        pixmapCache->fill(LockedCellColor);
    } else {
        pixmapCache->fill(Qt::transparent);
    }
    
    
    QPixmap cable(Renderer::self()->cablesPixmap(width(),
                  ddirs, isConnected()));
    QPainter painter(pixmapCache);
    
    if (angle != 0) {
        qreal offset = width() / 2.0;
        painter.translate(offset, offset);
        painter.rotate(angle);
        painter.translate(-offset, -offset);
    }
    
    painter.drawPixmap(0, 0, cable);
    painter.resetMatrix();
    
    painter.drawPixmap(0, 0, *forgroundCache);
}

void Cell::mousePressEvent(QMouseEvent* e)
{
    // do nothing if there is an animation running
    //if (timeLine->state() == QTimeLine::Running) return;
    
    if (e->button() == Qt::LeftButton) {
        emit lClicked(iindex);
    } else if (e->button() == Qt::RightButton) {
        emit rClicked(iindex);
    } else if (e->button() == Qt::MidButton) {
        emit mClicked(iindex);
    }
}

void Cell::resizeEvent(QResizeEvent* e)
{
    forgroundChanged = true;
    delete pixmapCache;
    delete forgroundCache;
    pixmapCache = new QPixmap(e->size());
    forgroundCache = new QPixmap(e->size());
}

void Cell::animateRotation(bool clockWise) 
{
    // if there is already an animation running make a new animition
    // taking into account also the new click
    if (timeLine->state() == QTimeLine::Running) {
        totalRotation += clockWise ? 90 : -90;
        
        timeLine->setFrameRange(timeLine->currentFrame(), totalRotation);
        timeLine->stop();
        timeLine->setCurrentTime(0);
        
        timeLine->start();
    } else {
        rotationStart = angle;
        totalRotation = clockWise ? 90 : -90;
        
        timeLine->setFrameRange(0, totalRotation);
        
        timeLine->start();
    }
}

void Cell::rotateStep(int a)
{
    int newAngle = rotationStart + a;
    rotate(newAngle - angle);
    
    if (a == totalRotation) {
        emit connectionsChanged();
    }
}

void Cell::rotate(int a)
{
    angle += a;
    
    while (angle > 45) {
        angle -= 90;
        rotationStart -= 90;
        int newdirs = None;
        if (ddirs & Up) newdirs |= Right;
        if (ddirs & Right) newdirs |= Down;
        if (ddirs & Down) newdirs |= Left;
        if (ddirs & Left) newdirs |= Up;
        setDirs(Directions(newdirs));
    }
    
    while (angle < -45) {
        angle += 90;
        rotationStart += 90;
        int newdirs = None;
        if (ddirs & Up) newdirs |= Left;
        if (ddirs & Right) newdirs |= Up;
        if (ddirs & Down) newdirs |= Right;
        if (ddirs & Left) newdirs |= Down;
        setDirs(Directions(newdirs));
    }
    
    cableChanged = true;
    update();
}

#include "cell.moc"
