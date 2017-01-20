#pragma once

#include <QWidget>
#include <QPainter>
#include <QMouseEvent>
#include <cassert>
#include "R2Graph.h"

const int DX = 80;
const int DY = 80;
const int THIN_WIDTH = 1;
const int NORMAL_WIDTH = 2;
const int THICK_WIDTH = 3;
const int VERY_THICK_WIDTH = 5;
const int LINE_WIDTH = THICK_WIDTH;

const int ERASER_WIDTH = 32;

const int NUM_COLORS = 5;
extern const QColor strokeColors[NUM_COLORS];

static const int MODE_CALIBRATION = 0;
static const int MODE_NORMAL = 1;
static const int NUM_CALIBRATION_POINTS = 2;
static const int MAX_PAGES = 8;

class Stroke {
public:
    int color;
    int width;
    std::vector<I2Point> points;
    QPainterPath* qPath;
    bool finished;

    Stroke():
        color(Qt::black),
        width(1),
        points(),
        qPath(0),
        finished(false)
    {}

    Stroke(const Stroke& str):
        color(str.color),
        width(str.width),
        points(str.points),
        qPath(0),
        finished(str.finished)
    {
        if (str.qPath != 0)
            qPath = new QPainterPath(*(str.qPath));
    }

    ~Stroke() {
        if (qPath != 0)
            delete qPath;
    }

    Stroke& operator=(const Stroke& str) {
        color = str.color;
        width = str.width;
        points = str.points;
        if (qPath != 0) {
            delete qPath;
            qPath = 0;
        }
        if (str.qPath != 0) {
            qPath = new QPainterPath(*(str.qPath));
        }
        finished = str.finished;
        return *this;
    }

    int size() const {
        return (int) points.size();
    }

    void clear() {
        points.clear();
        if (qPath != 0) {
            delete qPath;
            qPath = 0;
        }
    }

    void push_back(const I2Point& p) {
        if (size() == 0) {
            if (qPath != 0)
                delete qPath;
            qPath = new QPainterPath();
            qPath->moveTo(QPointF(p.x, p.y));
            //... qPath->lineTo(QPointF(p.x, p.y)); // Single point
            points.push_back(p);
        } else if (p != points.back()) {
            assert(qPath != 0);
            points.push_back(p);
            qPath->lineTo(QPointF(p.x, p.y));
        }
    }

    void finalize() {
        /*...
        if (size() == 1) {
            assert(qPath != 0);
            qPath->lineTo(      // Single point
                points.back().x, points.back().y
            );
        }
        ...*/
        finished = true;
    }
};

class Action {
public:
    enum {
        START_CURVE,
        DRAW_CURVE,
        END_CURVE
    };

    int type;
    int color;
    int width;
    I2Point point;

    Action():
        type(START_CURVE),
        color(0),
        width(LINE_WIDTH),
        point()
    {}

    Action(int t, int c, int w, const I2Point& pnt):
        type(t),
        color(c),
        width(w),
        point(pnt)
    {}
};

class WhiteBoard: public QWidget {
    Q_OBJECT

private:
    QPointF map(QPointF) const;
    QPointF invMap(QPointF) const;

    double xmin, xmax, ymin, ymax;
    double xCoeff, yCoeff;

    QImage* image;
    int imageWidth;
    int imageHeight;

public:
    bool finished;
    bool initialUpdate;

    class Page {
    public:
        std::vector<Stroke> strokes;
    };

    Page pages[MAX_PAGES];
    int currentPage;

    Stroke myDrawing;
    bool myDrawingActive;

    int mode;                   // MODE_CALIBRATION / MODE_NORMAL
    int currentColor;           // current color index
    int currentWidth;           // current line width
    int lastColor;
    int lastWidth;
    I2Point calibrationPoints[NUM_CALIBRATION_POINTS];
    I2Point calibrationClicks[NUM_CALIBRATION_POINTS];
    int numCalibrationClicks;

    // Colors
    QColor whiteColor, blackColor, redColor, greenColor, blueColor;
    QColor buttonColor1, buttonColor2, buttonColor3;

    // Mapping
    double xIntercept, xSlope;
    double yIntercept, ySlope;

    void mapMousePoint(const I2Point& mousePoint, I2Point& windowPoint) const;
    void mapWindowPoint(const I2Point& mousePoint, I2Point& windowPoint) const;

    WhiteBoard(QWidget *parent = 0);
    ~WhiteBoard() {
        if (image != 0)
            delete image;
    }

    void drawInOffscreen();
    void drawLastCurveInOffscreen();
    void drawStroke(QPainter* qp, Stroke& str);
    void drawCalibration(QPainter* qp);
    void drawButtons(QPainter* qp);
    void drawCurrentLineType(QPainter* qp = 0);
    void drawButton(
        QPainter* qp,
        const I2Rectangle& rect, 
        const char* text,
        const QColor& fgColor,
        const QColor& bgColor
    );
    void drawLineButton(
        QPainter* qp,
        const I2Rectangle& rect, 
        int lineWidth,
        const QColor& fgColor,
        const QColor& bgColor
    );
    void drawLine(
        QPainter* qp,
        const I2Point& p0, const I2Point& p1
    );
    void drawLineStrip(
        QPainter* qp,
        const I2Point* nodes,
        int numNodes
    );

    void processAction(const Action& a);
    void init();
    void allocateImage();
    void clearImage();

protected:
    // Virtual methods
    void paintEvent(QPaintEvent* event);
    void resizeEvent(QResizeEvent* event);
    void mousePressEvent(QMouseEvent* event);
    void mouseReleaseEvent(QMouseEvent* event);
    void mouseMoveEvent(QMouseEvent* event);
};
