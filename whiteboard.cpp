//
// File "whiteboard.cpp"
//
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#include <vector>

#include "gwindow.h"

static const int DX = 80;
static const int DY = 80;
static const int THIN_WIDTH = 1;
static const int NORMAL_WIDTH = 2;
static const int THICK_WIDTH = 3;
static const int VERY_THICK_WIDTH = 5;
static const int LINE_WIDTH = THICK_WIDTH;

static const int ERASER_WIDTH = 15;

static const int NUM_COLORS = 5;
/*...
static const char* const strokeColors[NUM_COLORS] = {
    "black",
    "blue",
    "red",
    "SeaGreen",
    "white"     // Eraser color
};
...*/
static unsigned int strokeColors[NUM_COLORS] = {
    0,
    1,
    2,
    3,
    4
};
const int BLACK_COLOR_IDX = 0;
const int BLUE_COLOR_IDX = 1;
const int RED_COLOR_IDX = 2;
const int GREEN_COLOR_IDX = 3;
const int ERASER_COLOR_IDX = 4;

class Stroke {
public:
    int color;
    int width;
    std::vector<I2Point> points;
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

static const int MODE_CALIBRATION = 0;
static const int MODE_NORMAL = 1;
static const int NUM_CALIBRATION_POINTS = 2;
static const int MAX_PAGES = 8;

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

//--------------------------------------------------
// Definition of our main class "MyWindow"
//
class MyWindow: public GWindow {    // Our main class
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
    unsigned int whiteColor, blackColor, redColor, greenColor, blueColor;
    unsigned int buttonColor1, buttonColor2, buttonColor3;

    // Mapping
    double xIntercept, xSlope;
    double yIntercept, ySlope;

    void mapMousePoint(const I2Point& mousePoint, I2Point& windowPoint) const;
    void mapWindowPoint(const I2Point& mousePoint, I2Point& windowPoint) const;

    MyWindow();
    void drawStroke(const Stroke& str);
    void drawCalibration();
    void drawButtons();
    void drawButton(
        const I2Rectangle& rect, 
        const char* text,
        unsigned long fgColor,
        unsigned long bgColor
    );
    void drawLineButton(
        const I2Rectangle& rect, 
        int lineWidth,
        unsigned long fgColor,
        unsigned long bgColor
    );
    void processAction(const Action& a, bool myAction = true);
    void init();

    virtual void onExpose(XEvent& event);
    virtual void onKeyPress(XEvent& event);
    virtual void onButtonPress(XEvent& event);
    virtual void onButtonRelease(XEvent& event);
    virtual void onMotionNotify(XEvent& event);
    virtual bool onWindowClosing();
};

//----------------------------------------------------------
// Implementation of class "MyWindow"

MyWindow::MyWindow():
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
    //... calibrationPoints[1] = I2Point(x1, y0);
    //... calibrationPoints[2] = I2Point(x0, y1);
    //... calibrationPoints[3] = I2Point(x1, y1);
    calibrationPoints[1] = I2Point(x1, y1);
}

void MyWindow::init() {
    pages[currentPage].strokes.clear();
    myDrawingActive = false;
    redraw();
}

//
// Process the Expose event: draw in the window
//
void MyWindow::onExpose(XEvent& /* event */) {
    if (initialUpdate) {
        whiteColor = allocateColor("white");
        blackColor = allocateColor("black");
        redColor = allocateColor("red");
        greenColor = allocateColor("SeaGreen");
        blueColor = allocateColor("blue");

        buttonColor1 = allocateColor("LightGray");
        buttonColor2 = blackColor;
        buttonColor3 = allocateColor("SlateGray3");

        strokeColors[BLACK_COLOR_IDX] = blackColor;
        strokeColors[BLUE_COLOR_IDX] = blueColor;
        strokeColors[RED_COLOR_IDX] = redColor;
        strokeColors[GREEN_COLOR_IDX] = greenColor;
        strokeColors[ERASER_COLOR_IDX] = whiteColor;

        initialUpdate = false;
    }

    // Erase a window
    setForeground(getBackground());
    fillRectangle(m_RWinRect);

    if (mode == MODE_CALIBRATION) {
        drawCalibration();
    } else {
        setLineWidth(currentWidth);     // Not necessary...
        for (
            unsigned int i = 0; 
            i < pages[currentPage].strokes.size(); 
            ++i
        ) {
            drawStroke(pages[currentPage].strokes[i]);
        }

        if (myDrawingActive)
            drawStroke(myDrawing);

        drawButtons();
    }
}

void MyWindow::drawStroke(const Stroke& str) {
    if (str.points.size() == 0)
        return;
    int c = str.color % NUM_COLORS;
    setForeground(strokeColors[c]);
    setLineWidth(str.width);

    /*...
    I2Point p = str.points[0];
    bool drawn = false;
    moveTo(p);
    for (unsigned int i = 1; i < str.points.size(); ++i) {
        if (str.points[i] != p) {
            drawLineTo(str.points[i]);
            p = str.points[i];
            drawn = true;
        }
    }
    if (!drawn) {
        // A single point
        I2Vector vx(1, 0);
        I2Vector vy(0, 1);
        drawLine(p-vx, p+vx);
        drawLine(p-vy, p+vy);
    }
    ...*/

    if (str.points.size() == 1) {
        // A single point
        I2Point p = str.points[0];
        I2Vector vx(1, 0);
        I2Vector vy(0, 1);
        drawLine(p-vx, p+vx);
        drawLine(p-vy, p+vy);
    } else {
        drawLineStrip(
            &(str.points[0]),
            (int) str.points.size()
        );
    }
}

void MyWindow::drawCalibration() {
    if (mode != MODE_CALIBRATION)
        return;

    setForeground("red");

    I2Vector dx(16, 0);
    I2Vector dy(0, 16);
    I2Point t = calibrationPoints[numCalibrationClicks];
    drawString(t - dy*2 - dx*2, "Click in cross:");

    setLineWidth(3);
    setForeground("blue");
    drawLine(t - dx, t + dx);
    drawLine(t - dy, t + dy);
}

void MyWindow::drawButtons() {
    drawButton(
        blackButtonRect,
        "Black",
        // blackColor,
        // buttonColor3
        whiteColor,
        blackColor
    );
    drawButton(
        redButtonRect,
        "Red",
        // redColor,
        // buttonColor3
        whiteColor,
        redColor
    );
    drawButton(
        greenButtonRect,
        "Green",
        whiteColor,
        greenColor
    );
    drawButton(
        blueButtonRect,
        "Blue",
        whiteColor,
        blueColor
    );
    drawButton(
        clearButtonRect,
        "Clear",
        blackColor,
        whiteColor
    );
    drawButton(
        eraseButtonRect,
        "Eraser",
        blackColor,
        buttonColor3
    );
    drawButton(
        calibrateButtonRect,
        "Calibrate",
        blackColor,
        buttonColor3
    );
    drawButton(
        quitButtonRect,
        "Quit",
        blackColor,
        buttonColor3
    );

    drawLineButton(
        thinButtonRect,
        THIN_WIDTH,
        blackColor,
        whiteColor
    );
    drawLineButton(
        normalButtonRect,
        NORMAL_WIDTH,
        blackColor,
        whiteColor
    );
    drawLineButton(
        thickButtonRect,
        THICK_WIDTH,
        blackColor,
        whiteColor
    );
    drawLineButton(
        veryThickButtonRect,
        VERY_THICK_WIDTH,
        blackColor,
        whiteColor
    );

    setForeground(strokeColors[currentColor]);
    setLineWidth(currentWidth);
    int x = quitButtonRect.right() + BUTTON_SKIP;
    int y = (
            calibrateButtonRect.top() +
            calibrateButtonRect.bottom()
        ) / 2 - 2;
    drawLine(
        x, y, x + BUTTON_WIDTH, y
    );
}

void MyWindow::drawButton(
    const I2Rectangle& rect, 
    const char* text,
    unsigned long fgColor,
    unsigned long bgColor
) {
    setLineWidth(1);

    // setForeground(buttonColor3);
    setForeground(bgColor);
    fillRectangle(rect);

    setForeground(buttonColor1);
    moveTo(rect.left(), rect.bottom());
    drawLineTo(rect.left(), rect.top());
    drawLineTo(rect.right(), rect.top());

    setForeground(buttonColor2);
    drawLineTo(rect.right(), rect.bottom());
    drawLineTo(rect.left(), rect.bottom());

    setForeground(fgColor);
    drawString(
        rect.left() + 8,
        rect.top() + 14,
        text
    );
}

void MyWindow::drawLineButton(
    const I2Rectangle& rect, 
    int lineWidth,
    unsigned long fgColor,
    unsigned long bgColor
) {
    setLineWidth(1);

    setForeground(bgColor);
    fillRectangle(rect);

    setForeground(buttonColor1);
    moveTo(rect.left(), rect.bottom());
    drawLineTo(rect.left(), rect.top());
    drawLineTo(rect.right(), rect.top());

    setForeground(buttonColor2);
    drawLineTo(rect.right(), rect.bottom());
    drawLineTo(rect.left(), rect.bottom());

    setForeground(fgColor);
    setLineWidth(lineWidth);
    int y = (rect.top() + rect.bottom())/2;
    drawLine(
        rect.left() + 2, y,
        rect.right() - 2, y
    );
}

void MyWindow::mapMousePoint(const I2Point& mousePoint, I2Point& windowPoint) const {
    windowPoint.x = (int)(
        xIntercept + (double) mousePoint.x * xSlope
        + 0.49
    );
    windowPoint.y = (int)(
        yIntercept + (double) mousePoint.y * ySlope
        + 0.49
    );
}

//
// Process the KeyPress event: 
// if "q" is pressed, then close the window
//
void MyWindow::onKeyPress(XEvent& event) {
    KeySym key;
    char keyName[256];
    int nameLen = XLookupString(&(event.xkey), keyName, 255, &key, 0);
    // printf("KeyPress: keycode=0x%x, state=0x%x, KeySym=0x%x\n",
    //    event.xkey.keycode, event.xkey.state, (int) key);
    if (nameLen > 0) {
        keyName[nameLen] = 0;
        // printf("\"%s\" button pressed.\n", keyName);
        if (keyName[0] == 'q') { // quit => close window
            destroyWindow();
        } else if (keyName[0] == 'i' || keyName[0] == 'I') { // 'i' => clear window
            init();
        } else if (keyName[0] == 'c' || keyName[0] == 'C') { // 'c' => calibrate
            mode = MODE_CALIBRATION;
            numCalibrationClicks = 0;
            redraw();
        }
    }
}

// Process mouse click
void MyWindow::onButtonPress(XEvent& event) {
    int x = event.xbutton.x;
    int y = event.xbutton.y;
    //... unsigned int mouseButton = event.xbutton.button;

    /*
    printf("Mouse click: x=%d, y=%d, button=%d\n", x, y, mouseButton);
    printf(
        "    x_root=%d, y_root=%d state=%x\n",
        event.xbutton.x_root, event.xbutton.y_root, event.xbutton.state
    );
    */

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
        redraw();
        return;
    }

    I2Point wp;
    mapMousePoint(t, wp);

    if (blackButtonRect.contains(wp)) {
        currentColor = BLACK_COLOR_IDX;
        lastColor = currentColor;
        currentWidth = lastWidth;
        myDrawing.color = currentColor;
        redraw();
        return;
    }
    if (blueButtonRect.contains(wp)) {
        currentColor = BLUE_COLOR_IDX;
        lastColor = currentColor;
        currentWidth = lastWidth;
        myDrawing.color = currentColor;
        redraw();
        return;
    }
    if (redButtonRect.contains(wp)) {
        currentColor = RED_COLOR_IDX;
        lastColor = currentColor;
        myDrawing.color = currentColor;
        currentWidth = lastWidth;
        redraw();
        return;
    }
    if (greenButtonRect.contains(wp)) {
        currentColor = GREEN_COLOR_IDX;
        lastColor = currentColor;
        myDrawing.color = currentColor;
        currentWidth = lastWidth;
        redraw();
        return;
    }
    if (clearButtonRect.contains(wp)) {
        currentColor = BLACK_COLOR_IDX; // Black
        lastColor = currentColor;
        currentWidth = LINE_WIDTH;
        init();
        redraw();
        return;
    }
    if (calibrateButtonRect.contains(wp)) {
        mode = MODE_CALIBRATION;
        numCalibrationClicks = 0;
        redraw();
        return;
    }
    if (eraseButtonRect.contains(wp)) {
        currentColor = ERASER_COLOR_IDX;
        currentWidth = ERASER_WIDTH;
        myDrawing.color = currentColor;
        myDrawing.width = currentWidth;
        redraw();
        return;
    }

    if (thinButtonRect.contains(wp)) {
        currentWidth = THIN_WIDTH;
        lastWidth = currentWidth;
        currentColor = lastColor;
        myDrawing.color = currentColor;
        myDrawing.width = currentWidth;
        redraw();
        return;
    }

    if (normalButtonRect.contains(wp)) {
        currentWidth = NORMAL_WIDTH;
        lastWidth = currentWidth;
        currentColor = lastColor;
        myDrawing.color = currentColor;
        myDrawing.width = currentWidth;
        redraw();
        return;
    }

    if (thickButtonRect.contains(wp)) {
        currentWidth = THICK_WIDTH;
        lastWidth = currentWidth;
        currentColor = lastColor;
        myDrawing.color = currentColor;
        myDrawing.width = currentWidth;
        redraw();
        return;
    }

    if (veryThickButtonRect.contains(wp)) {
        currentWidth = VERY_THICK_WIDTH;
        lastWidth = currentWidth;
        currentColor = lastColor;
        myDrawing.color = currentColor;
        myDrawing.width = currentWidth;
        redraw();
        return;
    }

    if (quitButtonRect.contains(wp)) {
        destroyWindow();
        return;
    }

    Action a(
        Action::START_CURVE,
        currentColor,
        currentWidth,
        wp
    );
    /*...
    if (mouseButton == Button2) {
        a.color = 1;
    } else if (mouseButton == Button3) {
        a.color = 2;
    }
    ...*/

    processAction(a, true);
}

void MyWindow::onButtonRelease(XEvent& event) {
    if (mode == MODE_CALIBRATION)
        return;

    int x = event.xbutton.x;
    int y = event.xbutton.y;
    I2Point t(x, y);
    I2Point wp;
    mapMousePoint(t, wp);
    Action a(
        Action::END_CURVE,
        0,
        0,
        wp
    );
    processAction(a, true);
}

void MyWindow::onMotionNotify(XEvent& event) {
    if (!myDrawingActive)
        return;
    int x = event.xbutton.x;
    int y = event.xbutton.y;
    I2Point t(x, y);
    I2Point wp;
    mapMousePoint(t, wp);
    Action a(
        Action::DRAW_CURVE,
        0,
        0,
        wp
    );
    processAction(a, true);
}

bool MyWindow::onWindowClosing() {
    return true;
}

void MyWindow::processAction(const Action& a, bool /* myAction = true */) {
    Stroke* curve = &myDrawing;
    bool* drawingActive = &myDrawingActive;

    if (a.type == Action::START_CURVE) {

        /*
        printf(
            "Action: START_CURVE, point=(%d, %d)\n",
            a.point.x, a.point.y
        );
        */

        if (*drawingActive && curve->points.size() > 0) {
            pages[currentPage].strokes.push_back(*curve);
            curve->points.clear();
        }
        curve->color = a.color;
        curve->width = a.width;
        curve->points.push_back(a.point);
        *drawingActive = true;
        drawStroke(*curve);
    } else if (a.type == Action::DRAW_CURVE) {
        if (!*drawingActive)
            return;

        /*
        printf(
            "Action: DRAW_CURVE, point=(%d, %d)\n",
            a.point.x, a.point.y
        );
        */

        int s = (int) curve->points.size();
        if (s <= 2) {
            curve->points.push_back(a.point);
            drawStroke(*curve);
        } else {
            I2Point lastPoint = curve->points[s-1];
            if (a.point != lastPoint) {
                setForeground(strokeColors[curve->color]);
                setLineWidth(curve->width);
                curve->points.push_back(a.point);
                //... drawLine(lastPoint, a.point);
                drawLineStrip(
                   &(curve->points[s-3]), 3
                );
            }
        }
    } else if (a.type == Action::END_CURVE) {

        /*
        printf(
            "Action: END_CURVE, point=(%d, %d)\n",
            a.point.x, a.point.y
        );
        */

        if (*drawingActive && curve->points.size() > 0) {

            /*
            printf(
                "Action: END_CURVE, drawing...\n"
            );
            */

            pages[currentPage].strokes.push_back(*curve);
            drawStroke(*curve);

            curve->points.clear();
        }
        *drawingActive = false;
        drawButtons();
    }
}

//
// End of class MyWindow implementation
//----------------------------------------------------------

/////////////////////////////////////////////////////////////
// Main: initialize X, create an instance of MyWindow class,
//       and start the message loop
int main() {
    // Initialize X stuff
    if (!GWindow::initX()) {
        printf("Could not connect to X-server.\n");
        exit(1);
    }

    MyWindow w;
    const char* windowTitle = "White Board";

    w.createWindow(
        I2Rectangle(        // Window frame rectangle:
            I2Point(0, 0),          // left-top corner
            GWindow::screenMaxX(),  // width
            GWindow::screenMaxY()   // height
        ),
        R2Rectangle(                // Coordinate rectangle:
            R2Point(-12., -9.),     //     bottom-right corner
            24., 18.                //     width, height
        ),
        windowTitle
    );
    w.setBackground("white");

    //... GWindow::messageLoop();
    // Message loop, animation
    XEvent e;
    while (GWindow::m_NumCreatedWindows > 0) {
        if (GWindow::getNextEvent(e)) {
            GWindow::dispatchEvent(e);
        } else {
            // Sleep a bit (we use select for sleeping)
            timeval dt;
            dt.tv_sec = 0;
            dt.tv_usec = 10000; // sleeping time 0.01 sec
            select(1, 0, 0, 0, &dt);
        }
    }

    GWindow::closeX();
    return 0;
}
