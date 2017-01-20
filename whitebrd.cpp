#include <QApplication>
#include "whitebrd.h"
#include <vector>
#include <cassert>

const QColor strokeColors[NUM_COLORS] = {
    Qt::black,
    Qt::blue,
    Qt::red,
    Qt::darkGreen,
    Qt::white
};

const int BLACK_COLOR_IDX = 0;
const int BLUE_COLOR_IDX = 1;
const int RED_COLOR_IDX = 2;
const int GREEN_COLOR_IDX = 3;
const int ERASER_COLOR_IDX = 4;

// Positions of buttons
static int BUTTON_WIDTH = 70;
static int BUTTON_WIDTH2 = BUTTON_WIDTH/2;
static int BUTTON_HEIGHT = 20;
static int BUTTON_SKIP = 8;
static int BUTTON_DX = BUTTON_WIDTH + BUTTON_SKIP;
static int BUTTON_DX2 = BUTTON_WIDTH2 + BUTTON_SKIP;

static const I2Rectangle blackButtonRect(
    I2Point(10, 10),
    BUTTON_WIDTH, BUTTON_HEIGHT
);

static const I2Rectangle redButtonRect(
    I2Point(10 + BUTTON_DX, 10),
    BUTTON_WIDTH, BUTTON_HEIGHT
);

static const I2Rectangle blueButtonRect(
    I2Point(10 + 2*BUTTON_DX, 10),
    BUTTON_WIDTH, BUTTON_HEIGHT
);

static const I2Rectangle greenButtonRect(
    I2Point(10 + 3*BUTTON_DX, 10),
    BUTTON_WIDTH, BUTTON_HEIGHT
);

static const I2Rectangle clearButtonRect(
    I2Point(10 + 4*BUTTON_DX, 10),
    BUTTON_WIDTH, BUTTON_HEIGHT
);

static const I2Rectangle eraseButtonRect(
    I2Point(10 + 5*BUTTON_DX, 10),
    BUTTON_WIDTH, BUTTON_HEIGHT
);

static const I2Rectangle calibrateButtonRect(
    I2Point(10 + 6*BUTTON_DX, 10),
    BUTTON_WIDTH, BUTTON_HEIGHT
);

static const I2Rectangle thinButtonRect(
    I2Point(10 + 7*BUTTON_DX, 10),
    BUTTON_WIDTH2, BUTTON_HEIGHT
);

static const I2Rectangle normalButtonRect(
    I2Point(10 + 7*BUTTON_DX + BUTTON_DX2, 10),
    BUTTON_WIDTH2, BUTTON_HEIGHT
);

static const I2Rectangle thickButtonRect(
    I2Point(10 + 7*BUTTON_DX + 2*BUTTON_DX2, 10),
    BUTTON_WIDTH2, BUTTON_HEIGHT
);

static const I2Rectangle veryThickButtonRect(
    I2Point(10 + 7*BUTTON_DX + 3*BUTTON_DX2, 10),
    BUTTON_WIDTH2, BUTTON_HEIGHT
);

static const I2Rectangle quitButtonRect(
    I2Point(10 + 7*BUTTON_DX + 4*BUTTON_DX2, 10),
    BUTTON_WIDTH, BUTTON_HEIGHT
);

WhiteBoard::WhiteBoard(QWidget *parent /* = 0 */):
    QWidget(parent),
    image(0),
    imageWidth(0),
    imageHeight(0),
    finished(false),
    initialUpdate(true),
    currentPage(0),
    myDrawing(),
    myDrawingActive(false),
    mode(MODE_CALIBRATION),
    currentColor(BLACK_COLOR_IDX),
    currentWidth(THICK_WIDTH),
    lastColor(BLACK_COLOR_IDX),
    lastWidth(THICK_WIDTH),
    numCalibrationClicks(0),

    // Mapping
    xIntercept(0.), 
    xSlope(1.),
    yIntercept(0.),
    ySlope(1.)
{
    int x0 = 100, x1 = 500;
    int y0 = 100, y1 = 400;
    calibrationPoints[0] = I2Point(x0, y0);
    calibrationPoints[1] = I2Point(x1, y1);
}

QPointF WhiteBoard::map(QPointF p) const {
    return QPointF(
        (p.x() - xmin)*xCoeff,
        (ymax - p.y())*yCoeff
    );
}

QPointF WhiteBoard::invMap(QPointF p) const {
    return QPointF(
        xmin + p.x()/xCoeff,
        ymax - p.y()/yCoeff
    );
}

void WhiteBoard::mapMousePoint(const I2Point& mousePoint, I2Point& windowPoint) const {
    windowPoint.x = (int)(
        xIntercept + (double) mousePoint.x * xSlope
        + 0.49
    );
    windowPoint.y = (int)(
        yIntercept + (double) mousePoint.y * ySlope
        + 0.49
    );
}

void WhiteBoard::paintEvent(QPaintEvent*) {
    QPainter qp(this);
    qp.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    if (initialUpdate) {
        whiteColor = Qt::white;
        blackColor = Qt::black;
        redColor = Qt::red;
        greenColor = Qt::darkGreen;
        blueColor = Qt::blue;

        buttonColor1 = Qt::lightGray;
        buttonColor2 = blackColor;
        buttonColor3 = QColor(0x9f, 0xb6, 0xcd); // SlateGray3 #9fb6cd

        initialUpdate = false;
    }

    if (mode == MODE_CALIBRATION) {
        // Erase a window
        qp.setBrush(QBrush(Qt::white));
        qp.drawRect(0, 0, w, h);

        drawCalibration(&qp);
    } else {
        if (image != 0) {
            qp.drawImage(0, 0, *image);
        } else {
            for (
                unsigned int i = 0;
                i < pages[currentPage].strokes.size();
                ++i
            ) {
                drawStroke(&qp, pages[currentPage].strokes.at(i));
            }

            if (myDrawingActive)
                drawStroke(&qp, myDrawing);

            drawButtons(&qp);
        }
    }
}

void WhiteBoard::drawInOffscreen() {
    if (
        image == 0 || 
        imageWidth != width() || 
        imageHeight != height()
    ) {
        allocateImage();
    }

    QPainter qp(image);
    qp.setRenderHint(QPainter::Antialiasing);

    int w = width();
    int h = height();

    // Erase a window
    qp.setBrush(QBrush(Qt::white));
    qp.drawRect(0, 0, w, h);

    assert(mode != MODE_CALIBRATION);
    for (
        unsigned int i = 0;
        i < pages[currentPage].strokes.size();
        ++i
    ) {
        drawStroke(&qp, pages[currentPage].strokes.at(i));
    }

    if (myDrawingActive)
        drawStroke(&qp, myDrawing);

    drawButtons(&qp);
}

void WhiteBoard::drawLastCurveInOffscreen() {
    if (!myDrawingActive || image == 0)
        return;
    QPainter qp(image);
    qp.setRenderHint(QPainter::Antialiasing);
    drawStroke(&qp, myDrawing);
}

void WhiteBoard::mousePressEvent(QMouseEvent* event) {
    int x = event->x();
    int y = event->y();
    I2Point t(x, y);

    if (mode == MODE_CALIBRATION) {
        assert(numCalibrationClicks < NUM_CALIBRATION_POINTS);
        calibrationClicks[numCalibrationClicks] = t;
        ++numCalibrationClicks;

        if (numCalibrationClicks == NUM_CALIBRATION_POINTS) {
            int lastClick = NUM_CALIBRATION_POINTS - 1;
            if (
                t.x == calibrationPoints[0].x ||
                t.y == calibrationPoints[0].y
            )
                return;

            xSlope =
                (double)(calibrationPoints[lastClick].x - calibrationPoints[0].x) /
                (double)(calibrationClicks[lastClick].x - calibrationClicks[0].x);
            ySlope =
                (double)(calibrationPoints[lastClick].y - calibrationPoints[0].y) /
                (double)(calibrationClicks[lastClick].y - calibrationClicks[0].y);
            // wx = wx0 + (cx - cx0)*sx =
            //      (wx0 - cx0*sx) + cx*sx;
            xIntercept = calibrationPoints[0].x - 
                calibrationClicks[0].x * xSlope;
            yIntercept = calibrationPoints[0].y -
                calibrationClicks[0].y * ySlope;
        }

        if (numCalibrationClicks >= NUM_CALIBRATION_POINTS)
            mode = MODE_NORMAL;
        update();
        return;
    }

    I2Point wp;
    mapMousePoint(t, wp);

    if (blackButtonRect.contains(wp)) {
        currentColor = BLACK_COLOR_IDX;
        lastColor = currentColor;
        currentWidth = lastWidth;
        myDrawing.color = currentColor;
        drawCurrentLineType();
        update();
        return;
    }
    if (blueButtonRect.contains(wp)) {
        currentColor = BLUE_COLOR_IDX;
        lastColor = currentColor;
        currentWidth = lastWidth;
        myDrawing.color = currentColor;
        drawCurrentLineType();
        update();
        return;
    }
    if (redButtonRect.contains(wp)) {
        currentColor = RED_COLOR_IDX;
        lastColor = currentColor;
        myDrawing.color = currentColor;
        currentWidth = lastWidth;
        drawCurrentLineType();
        update();
        return;
    }
    if (greenButtonRect.contains(wp)) {
        currentColor = GREEN_COLOR_IDX;
        lastColor = currentColor;
        myDrawing.color = currentColor;
        currentWidth = lastWidth;
        drawCurrentLineType();
        update();
        return;
    }
    if (clearButtonRect.contains(wp)) {
        currentColor = BLACK_COLOR_IDX; // Black
        lastColor = currentColor;
        currentWidth = LINE_WIDTH;
        drawCurrentLineType();
        init();
        update();
        return;
    }
    if (calibrateButtonRect.contains(wp)) {
        mode = MODE_CALIBRATION;
        numCalibrationClicks = 0;
        update();
        return;
    }
    if (eraseButtonRect.contains(wp)) {
        currentColor = ERASER_COLOR_IDX;
        currentWidth = ERASER_WIDTH;
        myDrawing.color = currentColor;
        myDrawing.width = currentWidth;
        update();
        return;
    }

    if (thinButtonRect.contains(wp)) {
        currentWidth = THIN_WIDTH;
        lastWidth = currentWidth;
        currentColor = lastColor;
        myDrawing.color = currentColor;
        myDrawing.width = currentWidth;
        drawCurrentLineType();
        update();
        return;
    }

    if (normalButtonRect.contains(wp)) {
        currentWidth = NORMAL_WIDTH;
        lastWidth = currentWidth;
        currentColor = lastColor;
        myDrawing.color = currentColor;
        myDrawing.width = currentWidth;
        drawCurrentLineType();
        update();
        return;
    }

    if (thickButtonRect.contains(wp)) {
        currentWidth = THICK_WIDTH;
        lastWidth = currentWidth;
        currentColor = lastColor;
        myDrawing.color = currentColor;
        myDrawing.width = currentWidth;
        drawCurrentLineType();
        update();
        return;
    }

    if (veryThickButtonRect.contains(wp)) {
        currentWidth = VERY_THICK_WIDTH;
        lastWidth = currentWidth;
        currentColor = lastColor;
        myDrawing.color = currentColor;
        myDrawing.width = currentWidth;
        drawCurrentLineType();
        update();
        return;
    }

    if (quitButtonRect.contains(wp)) {
        QApplication::instance()->quit();
        return;
    }

    Action a(
        Action::START_CURVE,
        currentColor,
        currentWidth,
        wp
    );

    processAction(a);
}

void WhiteBoard::mouseReleaseEvent(QMouseEvent* event) {
    if (mode == MODE_CALIBRATION)
        return;

    int x = event->x();
    int y = event->y();
    I2Point t(x, y);

    I2Point wp;
    mapMousePoint(t, wp);
    Action a(
        Action::END_CURVE,
        0,
        0,
        wp
    );
    processAction(a);
}

void WhiteBoard::mouseMoveEvent(QMouseEvent* event) {
    if (!myDrawingActive)
        return;

    int x = event->x();
    int y = event->y();
    I2Point t(x, y);
    I2Point wp;
    mapMousePoint(t, wp);
    Action a(
        Action::DRAW_CURVE,
        0,
        0,
        wp
    );
    processAction(a);
}

void WhiteBoard::resizeEvent(QResizeEvent* /* event */) {
    if (image != 0) {
        delete image; image = 0;
    }
    allocateImage();
    if (mode != MODE_CALIBRATION)
        drawInOffscreen();
}

void WhiteBoard::processAction(const Action& a) {
    Stroke* curve = &myDrawing;
    //... QPainter qp(this);
    //... qp.begin(this);
    //... qp.setRenderHint(QPainter::Antialiasing);

    if (a.type == Action::START_CURVE) {

        /*
        printf(
            "Action: START_CURVE, point=(%d, %d)\n",
            a.point.x, a.point.y
        );
        */

        if (myDrawingActive && curve->size() > 0) {
            pages[currentPage].strokes.push_back(*curve);
            curve->clear();
        }
        curve->color = a.color;
        curve->width = a.width;
        curve->push_back(a.point);
        myDrawingActive = true;
        //... drawLastCurveInOffscreen();
    } else if (a.type == Action::DRAW_CURVE) {
        if (!myDrawingActive) {
            return;
        }

        /*
        printf(
            "Action: DRAW_CURVE, point=(%d, %d)\n",
            a.point.x, a.point.y
        );
        */

        curve->push_back(a.point);
        drawLastCurveInOffscreen();

    } else if (a.type == Action::END_CURVE) {

        /*
        printf(
            "Action: END_CURVE, point=(%d, %d)\n",
            a.point.x, a.point.y
        );
        */

        if (myDrawingActive && curve->size() > 0) {
            curve->push_back(a.point);
            curve->finalize();
            //... drawLastCurveInOffscreen();

            /*
            printf(
                "Action: END_CURVE, drawing...\n"
            );
            */

            pages[currentPage].strokes.push_back(*curve);
            drawInOffscreen();

            curve->clear();
        }
        myDrawingActive = false;
        //... drawButtons(&qp);
    }

    update();
}

void WhiteBoard::drawLine(
    QPainter* qp,
    const I2Point& p0, const I2Point& p1
) {
    QPointF t0(p0.x, p0.y);
    QPointF t1(p1.x, p1.y);
    qp->drawLine(t0, t1);
}

/*...
void WhiteBoard::drawLineStrip(
    QPainter* qp,
    const I2Point* nodes,
    int numNodes
) {
    if (numNodes <= 0)
        return;

    QPointF p(nodes[0].x, nodes[0].y);
    QPainterPath path;

    path.moveTo(p);
    for (int i = 1; i < numNodes; ++i) {
        p.setX(nodes[i].x);
        p.setY(nodes[i].y);
        path.lineTo(p);
    }

    qp->strokePath(path, qp->pen());
}
...*/

void WhiteBoard::drawStroke(QPainter* qp, Stroke& str) {
    if (str.size() == 0)
        return;
    int c = str.color % NUM_COLORS;
    QPen pen(strokeColors[c]);
    pen.setWidth(str.width);

    if (str.size() == 1) {
        if (str.finished) {
            // A single point
            qp->setPen(pen);
            I2Point p = str.points.at(0);
            I2Vector vx(1, 0);
            I2Vector vy(0, 1);
            drawLine(qp, p-vx, p+vx);
            drawLine(qp, p-vy, p+vy);
        }
    } else {
        /*
        drawLineStrip(
            qp,
            &(str.points.at(0)),
            (int) str.size()
        );
        */
        assert(str.qPath != 0);
        qp->strokePath(*(str.qPath), pen);
    }
}

void WhiteBoard::init() {
    pages[currentPage].strokes.clear();
    myDrawingActive = false;
    if (image != 0)
        clearImage();
    update();
}

void WhiteBoard::drawCalibration(QPainter* qp) {
    if (mode != MODE_CALIBRATION)
        return;

    QPen pen(Qt::red);
    pen.setWidth(1);
    qp->setPen(pen);

    I2Vector dx(16, 0);
    I2Vector dy(0, 16);
    I2Point t = calibrationPoints[numCalibrationClicks];

    QPointF dxx(16., 0.);
    QPointF dyy(0., 16.);
    QPointF tt(t.x, t.y);

    qp->drawText(tt - dyy*2. - dxx*2., "Click in cross:");

    QPen pen1(Qt::blue);
    pen1.setWidth(3);
    qp->setPen(pen1);

    drawLine(qp, t - dx, t + dx);
    drawLine(qp, t - dy, t + dy);
}

void WhiteBoard::drawButtons(QPainter* qp) {
    drawButton(
        qp,
        blackButtonRect,
        "Black",
        // blackColor,
        // buttonColor3
        whiteColor,
        blackColor
    );
    drawButton(
        qp,
        redButtonRect,
        "Red",
        // redColor,
        // buttonColor3
        whiteColor,
        redColor
    );
    drawButton(
        qp,
        greenButtonRect,
        "Green",
        whiteColor,
        greenColor
    );
    drawButton(
        qp,
        blueButtonRect,
        "Blue",
        whiteColor,
        blueColor
    );
    drawButton(
        qp,
        clearButtonRect,
        "Clear",
        blackColor,
        whiteColor
    );
    drawButton(
        qp,
        eraseButtonRect,
        "Eraser",
        blackColor,
        buttonColor3
    );
    drawButton(
        qp,
        calibrateButtonRect,
        "Calibrate",
        blackColor,
        buttonColor3
    );
    drawButton(
        qp,
        quitButtonRect,
        "Quit",
        blackColor,
        buttonColor3
    );

    drawLineButton(
        qp,
        thinButtonRect,
        THIN_WIDTH,
        blackColor,
        whiteColor
    );
    drawLineButton(
        qp,
        normalButtonRect,
        NORMAL_WIDTH,
        blackColor,
        whiteColor
    );
    drawLineButton(
        qp,
        thickButtonRect,
        THICK_WIDTH,
        blackColor,
        whiteColor
    );
    drawLineButton(
        qp,
        veryThickButtonRect,
        VERY_THICK_WIDTH,
        blackColor,
        whiteColor
    );

    /*... Moved to drawCurrentLineType
    QPen pen(strokeColors[currentColor]);
    pen.setWidth(currentWidth);
    qp->setPen(pen);

    int x = quitButtonRect.right() + BUTTON_SKIP;
    int y = (
            calibrateButtonRect.top() +
            calibrateButtonRect.bottom()
        ) / 2 - 2;
    drawLine(
        qp,
        I2Point(x, y), 
        I2Point(x + BUTTON_WIDTH, y)
    );
    ...*/
    drawCurrentLineType(qp);
}

void WhiteBoard::drawCurrentLineType(QPainter* qpnt /* = 0 */) {
    QPainter* qp = qpnt;
    if (qpnt == 0) {
        if (image == 0)
            return;
        qp = new QPainter(image);
        qp->setRenderHint(QPainter::Antialiasing);
    }

    int x = quitButtonRect.right() + BUTTON_SKIP;
    int y = (
            calibrateButtonRect.top() +
            calibrateButtonRect.bottom()
        ) / 2 - 2;

    // Erase the rectangle
    qp->setBrush(QBrush(Qt::white));
    qp->setPen(Qt::white);
    qp->drawRect(
        x-2, calibrateButtonRect.top() - 1,
        BUTTON_WIDTH+4, BUTTON_HEIGHT+2
    );

    QPen pen(strokeColors[currentColor]);
    pen.setWidth(currentWidth);
    qp->setPen(pen);

    drawLine(
        qp,
        I2Point(x, y), 
        I2Point(x + BUTTON_WIDTH, y)
    );

    if (qpnt == 0)
        delete qp;
}

void WhiteBoard::drawButton(
    QPainter* qp,
    const I2Rectangle& rect, 
    const char* text,
    const QColor& fgColor,
    const QColor& bgColor
) {
    qp->setBrush(QBrush(bgColor));
    qp->drawRect(
        rect.left(), rect.top(),
        rect.width(), rect.height()
    );

    QPen pen(buttonColor1);
    pen.setWidth(1);
    qp->setPen(pen);

    // moveTo(rect.left(), rect.bottom());
    // drawLineTo(rect.left(), rect.top());
    // drawLineTo(rect.right(), rect.top());
    drawLine(
        qp, 
        I2Point(rect.left(), rect.bottom()),
        I2Point(rect.left(), rect.top())
    );
    drawLine(
        qp, 
        I2Point(rect.left(), rect.top()),
        I2Point(rect.right(), rect.top())
    );

    QPen pen1(buttonColor2);
    pen1.setWidth(1);
    qp->setPen(pen1);
    // drawLineTo(rect.right(), rect.bottom());
    // drawLineTo(rect.left(), rect.bottom());
    drawLine(
        qp, 
        I2Point(rect.right(), rect.top()),
        I2Point(rect.right(), rect.bottom())
    );
    drawLine(
        qp, 
        I2Point(rect.right(), rect.bottom()),
        I2Point(rect.left(), rect.bottom())
    );

    QPen pen2(fgColor);
    pen2.setWidth(1);
    qp->setPen(pen2);
    qp->drawText(
        rect.left() + 8,
        rect.top() + 14,
        text
    );
}

void WhiteBoard::allocateImage() {
    int w = width();
    int h = height();
    if (
        image == 0 ||
        imageWidth != w || imageHeight != h
    ) {
        if (image != 0)
            delete image;
        image = new QImage(w, h, QImage::Format_RGB32);
        imageWidth = w;
        imageHeight = h;
    }

    clearImage();
}

void WhiteBoard::clearImage() {
    assert(image != 0);
    if (image == 0)
        return;

    // Erase an image and draw buttons
    QPainter qp(image);
    qp.setRenderHint(QPainter::Antialiasing);

    qp.setBrush(QBrush(Qt::white));
    qp.drawRect(0, 0, imageWidth, imageHeight);
    drawButtons(&qp);
}

void WhiteBoard::drawLineButton(
    QPainter* qp,
    const I2Rectangle& rect, 
    int lineWidth,
    const QColor& fgColor,
    const QColor& bgColor
) {
    qp->setBrush(QBrush(bgColor));
    qp->drawRect(
        rect.left(), rect.top(),
        rect.width(), rect.height()
    );

    QPen pen(buttonColor1);
    pen.setWidth(1);
    qp->setPen(pen);

    // moveTo(rect.left(), rect.bottom());
    // drawLineTo(rect.left(), rect.top());
    // drawLineTo(rect.right(), rect.top());
    drawLine(
        qp, 
        I2Point(rect.left(), rect.bottom()),
        I2Point(rect.left(), rect.top())
    );
    drawLine(
        qp, 
        I2Point(rect.left(), rect.top()),
        I2Point(rect.right(), rect.top())
    );

    // setForeground(buttonColor2);
    // drawLineTo(rect.right(), rect.bottom());
    // drawLineTo(rect.left(), rect.bottom());
    QPen pen1(buttonColor2);
    pen1.setWidth(1);
    qp->setPen(pen1);
    drawLine(
        qp, 
        I2Point(rect.right(), rect.top()),
        I2Point(rect.right(), rect.bottom())
    );
    drawLine(
        qp, 
        I2Point(rect.right(), rect.bottom()),
        I2Point(rect.left(), rect.bottom())
    );

    // setForeground(fgColor);
    // setLineWidth(lineWidth);
    QPen pen2(fgColor);
    pen2.setWidth(lineWidth);
    qp->setPen(pen2);

    int y = (rect.top() + rect.bottom())/2;
    drawLine(
        qp,
        I2Point(rect.left() + 2, y),
        I2Point(rect.right() - 2, y)
    );
}
