#include <X11/Xlib.h>
#include <X11/Xos.h>
#include <X11/Xutil.h>

#include <chrono>
#include <climits>	// for constant USHRT_MAX
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <thread>
#include <vector>

#define sleep(ms) std::this_thread::sleep_for(std::chrono::milliseconds(ms))
#define WINDOW_WIDTH 300
#define WINDOW_HEIGHT 300

// TODO: normalize color for XColor
void SetRGB(XColor& xcolor, unsigned short red, unsigned short green,
	unsigned short blue) {
	// FIXME: check if red > 255 or red < 0

	xcolor.red = (red * USHRT_MAX) / 255;
	xcolor.green = (green * USHRT_MAX) / 255;
	xcolor.blue = (blue * USHRT_MAX) / 255;
}

struct XWindow {
	Display* dpy;
	Window win;
	Visual* vis;
	int scr;
	unsigned short screen_width;
	unsigned short screen_height;
	Colormap colormap;
};

static XWindow xw;


void xinit(bool is_gui) {
	Window rootWin;
	unsigned long bgcolor;
	int depth;
	XSetWindowAttributes attrs;
	XSizeHints sizeHints;
	XClassHint classHint;
	char className[] = "example";
	char resName[] = "example";


	xw.dpy = XOpenDisplay(NULL);
	if (!xw.dpy) {
		std::cerr << "cant open connect to x-server" << std::endl;
		exit(1);
	}

	xw.scr = DefaultScreen(xw.dpy);
	rootWin = RootWindow(xw.dpy, xw.scr);
	bgcolor = BlackPixel(xw.dpy, xw.scr);
	depth = DefaultDepth(xw.dpy, xw.scr);
	xw.vis = DefaultVisual(xw.dpy, xw.scr);


	Screen* screen = XScreenOfDisplay(xw.dpy, xw.scr);
	xw.screen_width = XWidthOfScreen(screen);
	xw.screen_height = XHeightOfScreen(screen);

	attrs.background_pixel = bgcolor;
	xw.win = XCreateWindow(xw.dpy, rootWin,
		(xw.screen_width >> 1) - (WINDOW_WIDTH >> 1),
		(xw.screen_height >> 1) - (WINDOW_HEIGHT >> 1), WINDOW_WIDTH,
		WINDOW_HEIGHT, 0, depth, InputOutput, xw.vis, CWBackPixel, &attrs);

	XStoreName(xw.dpy, xw.win, "chess board");

	sizeHints.flags = PMinSize | PMaxSize;
	sizeHints.min_width = WINDOW_WIDTH;
	sizeHints.min_height = WINDOW_HEIGHT;
	sizeHints.max_width = WINDOW_WIDTH;
	sizeHints.max_height = WINDOW_HEIGHT;
	XSetWMNormalHints(xw.dpy, xw.win, &sizeHints);

	classHint.res_name = resName;
	classHint.res_class = className;
	XSetClassHint(xw.dpy, xw.win, &classHint);
	XSelectInput(xw.dpy, xw.win, ExposureMask | KeyPressMask);

	xw.colormap = DefaultColormap(xw.dpy, xw.scr);

	if (is_gui) {
		XMapWindow(xw.dpy, xw.win);
		XFlush(xw.dpy);
	}
}

struct Point {
	int x;
	int y;
	void Print(const char str[]) {
		std::cout << str << x << " " << y << std::endl;
	}
};

class ChessCell {
public:
	Point point;
	unsigned short x;  // relative position on window
	unsigned short y;  // relative position on window
	XColor* color;
	bool isWhite;
};

class Figure;


/* CHESS BOARD */
class ChessBoard {
	unsigned short cell_size;
	unsigned short cell_number;
	ChessCell** cells;
	XColor black_cell;
	XColor white_cell;

	Figure* figure;

public:
	ChessBoard(const unsigned short& cell_number);
	void initFigure(Figure* figure);
	ChessCell* getCell(unsigned int x, unsigned int y);
	unsigned short getCellSize();
	unsigned short getCellNumber();
	void Print();
	void Draw();
	~ChessBoard();
};


class Figure {
protected:
	std::vector<ChessCell*> steps;
	XColor curr_cell;
	XColor prev_white_cell;
	XColor prev_black_cell;
	ChessBoard* chessBoard;

public:
	Figure(ChessBoard* chessBoard, unsigned short x, unsigned short y) {
		this->chessBoard = chessBoard;
		steps.push_back(chessBoard->getCell(x, y));
		SetRGB(curr_cell, 245, 75, 66);
		SetRGB(prev_white_cell, 209, 209, 209);
		SetRGB(prev_black_cell, 153, 153, 153);
		prev_black_cell.flags = prev_white_cell.flags = curr_cell.flags =
			DoRed | DoGreen | DoBlue;
		XAllocColor(xw.dpy, xw.colormap, &curr_cell);
		XAllocColor(xw.dpy, xw.colormap, &prev_white_cell);
		XAllocColor(xw.dpy, xw.colormap, &prev_black_cell);

		steps.back()->color = &curr_cell;
	}
	virtual bool Move() = 0;
	virtual void Draw() = 0;
	virtual Point getLastMove() = 0;
};


/* HORSE */
class Horse : public Figure {
	const unsigned short moves_number = 8;
	const Point moves[8] = {
		{-2, -1}, {-1, -2}, {1, -2}, {2, -1}, {2, 1}, {1, 2}, {-1, 2}, {-2, 1}};

	bool checkExistPrev(Point current, const Point& move) {
		current.x += move.x;
		current.y += move.y;
		for (unsigned short i = 0; i < steps.size(); i++) {
			if (steps[i]->point.x == current.x &&
				steps[i]->point.y == current.y) {
				return true;
			}
		}
		return false;
	}

	unsigned short checkAdditional(const Point& current) {
		unsigned short resolve_moves = 0;
		for (unsigned short i = 0; i < moves_number; i++) {
			if (current.x + moves[i].x < 0 || current.y + moves[i].y < 0 ||
				current.x + moves[i].x >= chessBoard->getCellNumber() ||
				current.y + moves[i].y >= chessBoard->getCellNumber()) {
				continue;
			}

			if (checkExistPrev(current, moves[i])) {
				continue;
			}
			resolve_moves++;
		}
		return resolve_moves;
	}

public:
	Horse(ChessBoard* chessBoard, unsigned short x, unsigned short y)
		: Figure(chessBoard, x, y) {}

	bool Move() override {
		// find the best move
		Point temp_point = steps.back()->point;
		unsigned short min_moves = 9;
		unsigned short changer;
		Point best_move;


		// steps.back()->point is pointer to current position
		for (unsigned short i = 0; i < moves_number; i++) {
			temp_point = {steps.back()->point.x + moves[i].x,
				steps.back()->point.y + moves[i].y};

			// check bounds
			if (temp_point.x < 0 || temp_point.y < 0 ||
				temp_point.x >= chessBoard->getCellNumber() ||
				temp_point.y >= chessBoard->getCellNumber()) {
				continue;
			}

			// check previous moves
			if (checkExistPrev(steps.back()->point, moves[i])) {
				continue;
			}

			// additional check
			changer = checkAdditional(temp_point);
			if (changer < min_moves) {
				min_moves = changer;
				best_move = temp_point;
			}
		}
		if (min_moves == 9) {
			return false;
		}

		// move to best cell if it exists
		steps.back()->color =
			(steps.back()->isWhite) ? &prev_white_cell : &prev_black_cell;

		steps.push_back(chessBoard->getCell(best_move.x, best_move.y));
		steps.back()->color = &curr_cell;

		return true;
	}
	void Draw() override {
		GC gc = XCreateGC(xw.dpy, xw.win, 0, NULL);
		for (unsigned short i = 0; i < steps.size(); i++) {
			XSetForeground(xw.dpy, gc, steps[i]->color->pixel);
			XFillRectangle(xw.dpy, xw.win, gc, steps[i]->x, steps[i]->y,
				chessBoard->getCellSize(), chessBoard->getCellSize());
		}

		XFreeGC(xw.dpy, gc);
		XFlush(xw.dpy);
	}

	Point getLastMove() { return steps.back()->point; }
};


class Rook : public Figure {
	const unsigned short moves_number = 4;
	const Point moves[4] = {{-1, 0}, {0, -1}, {1, 0}, {0, 1}};

	bool checkExistPrev(Point current, const Point& move) {
		current.x += move.x;
		current.y += move.y;
		for (unsigned short i = 0; i < steps.size(); i++) {
			if (steps[i]->point.x == current.x &&
				steps[i]->point.y == current.y) {
				return true;
			}
		}
		return false;
	}

	unsigned short checkAdditional(const Point& current) {
		unsigned short resolve_moves = 0;
		for (unsigned short i = 0; i < moves_number; i++) {
			if (current.x + moves[i].x < 0 || current.y + moves[i].y < 0 ||
				current.x + moves[i].x >= chessBoard->getCellNumber() ||
				current.y + moves[i].y >= chessBoard->getCellNumber()) {
				continue;
			}

			if (checkExistPrev(current, moves[i])) {
				continue;
			}
			resolve_moves++;
		}
		return resolve_moves;
	}

public:
	Rook(ChessBoard* chessBoard, unsigned short x, unsigned short y)
		: Figure(chessBoard, x, y) {}

	bool Move() override {
		// find the best move
		Point temp_point = steps.back()->point;
		unsigned short min_moves = 5;
		unsigned short changer;
		Point best_move;


		// steps.back()->point is pointer to current position
		for (unsigned short i = 0; i < moves_number; i++) {
			temp_point = {steps.back()->point.x + moves[i].x,
				steps.back()->point.y + moves[i].y};

			// check bounds
			if (temp_point.x < 0 || temp_point.y < 0 ||
				temp_point.x >= chessBoard->getCellNumber() ||
				temp_point.y >= chessBoard->getCellNumber()) {
				continue;
			}

			// check previous moves
			if (checkExistPrev(steps.back()->point, moves[i])) {
				continue;
			}

			// additional check
			changer = checkAdditional(temp_point);
			if (changer < min_moves) {
				min_moves = changer;
				best_move = temp_point;
			}
		}
		if (min_moves == 5) {
			return false;
		}

		// move to best cell if it exists
		steps.back()->color =
			(steps.back()->isWhite) ? &prev_white_cell : &prev_black_cell;

		steps.push_back(chessBoard->getCell(best_move.x, best_move.y));
		steps.back()->color = &curr_cell;

		return true;
	}
	void Draw() override {
		GC gc = XCreateGC(xw.dpy, xw.win, 0, NULL);
		for (unsigned short i = 0; i < steps.size(); i++) {
			XSetForeground(xw.dpy, gc, steps[i]->color->pixel);
			XFillRectangle(xw.dpy, xw.win, gc, steps[i]->x, steps[i]->y,
				chessBoard->getCellSize(), chessBoard->getCellSize());
		}

		XFreeGC(xw.dpy, gc);
		XFlush(xw.dpy);
	}

	Point getLastMove() { return steps.back()->point; }
};


//---------------------------------------------------------------------------------
//---------------------------------- MAIN
//---------------------------------------------------------------------------------
int main() {
	srand(time(0));
	// initialize and show window
	unsigned short cell_number = 5;
	int figure_x = 0;
	int figure_y = 0;
	bool is_gui = false;
	unsigned short pick_figure = 1;

	std::cout << "GUI: ";
	std::cin >> is_gui;
	std::cout << "pick figure 1 - horse; 2 - rook: ";
	std::cin >> pick_figure;
	std::cout << "cell_number: ";
	std::cin >> cell_number;
	std::cout << "figure position x,y: ";
	std::cin >> figure_x >> figure_y;
	std::cout << figure_x << " " << figure_y << std::endl;

	if (cell_number < 3 || cell_number > 50) {
		std::cout << "cell_number too large" << std::endl;
		return 1;
	}

	if (figure_x < 0 || figure_x >= cell_number || figure_y < 0 ||
		figure_y >= cell_number) {
		std::cout << "position doesn't correct" << std::endl;
		return 1;
	}


	if (!is_gui) {
		xinit(is_gui);
		ChessBoard chessBoard(cell_number);
		Figure *figure;

		if (pick_figure == 1) {
			figure = new Horse(&chessBoard, figure_x, figure_y);
		} else {
			figure = new Rook(&chessBoard, figure_x, figure_y);
		}
		chessBoard.initFigure(figure);
		while (figure->Move()) {
			figure->getLastMove().Print("move: ");
		}
		delete figure;
		return 0;
	}


	xinit(is_gui);

	ChessBoard chessBoard(cell_number);
	Figure *figure;
	if (pick_figure == 1) {
		figure = new Horse(&chessBoard, figure_x, figure_y);
	} else {
		figure = new Rook(&chessBoard, figure_x, figure_y);
	}

	chessBoard.initFigure(figure);
	// chessBoard.Print();


	XEvent report;
	while (true) {
		XNextEvent(xw.dpy, &report);
		switch (report.type) {
			case Expose:
				if (report.xexpose.count != 0) break;
				chessBoard.Draw();
				break;
			case KeyPress:
				// std::cout << "\033\[1;38;5;196mkey pres\033\[0m" <<
				// std::endl;
				if (figure->Move()) {
					figure->getLastMove().Print("move: ");
					figure->Draw();
				} else {
					std::cout << "\033[0;38;5;226mcant find move\033[0m"
							  << std::endl;
				}

				// chessBoard.Draw();

				break;
				// XCloseDisplay(xw.dpy);
				// return 0;
		}
	}

	XCloseDisplay(xw.dpy);
	return 0;
}


ChessBoard::ChessBoard(const unsigned short& cell_number) {
	this->cell_number = cell_number;
	cells = new ChessCell*[cell_number];
	this->cell_size = WINDOW_WIDTH / cell_number;


	SetRGB(black_cell, 119, 149, 86);
	SetRGB(white_cell, 235, 236, 208);
	white_cell.flags = black_cell.flags = DoRed | DoGreen | DoBlue;
	XAllocColor(xw.dpy, xw.colormap, &black_cell);
	XAllocColor(xw.dpy, xw.colormap, &white_cell);

	int padding = (WINDOW_WIDTH - (cell_size * cell_number)) / 2;
	int temp_x = padding;
	int temp_y = padding;

	bool isWhite = true;
	for (int i = 0; i < cell_number; i++) {
		cells[i] = new ChessCell[cell_number];

		isWhite = !(i % 2);
		for (int j = 0; j < cell_number; j++) {
			cells[i][j].x = temp_x;
			cells[i][j].y = temp_y;
			cells[i][j].point = {j, i};

			cells[i][j].color = (isWhite) ? &white_cell : &black_cell;
			cells[i][j].isWhite = isWhite;
			isWhite = !isWhite;

			temp_x += cell_size;
		}
		temp_y += cell_size;
		temp_x = padding;
	}
}

ChessCell* ChessBoard::getCell(unsigned int x, unsigned int y) {
	ChessCell* tempCell = new ChessCell();
	// tempCell->xi =
	tempCell->x = cells[y][x].x;
	tempCell->y = cells[y][x].y;
	tempCell->point = cells[y][x].point;
	tempCell->isWhite = cells[y][x].isWhite;
	return tempCell;
}

void ChessBoard::Print() {
	for (int i = 0; i < cell_number; ++i) {
		for (int j = 0; j < cell_number; ++j) {
			// std::cout << "{ " << cells[i][j].ix << ", " << cells[i][j].iy <<
			// " } ";
			std::cout << cells[i][j].isWhite;
		}
		std::cout << std::endl;
	}
}

void ChessBoard::Draw() {
	GC gc = XCreateGC(xw.dpy, xw.win, 0, NULL);

	for (int i = 0; i < cell_number; i++) {
		for (int j = 0; j < cell_number; j++) {
			XSetForeground(xw.dpy, gc, cells[i][j].color->pixel);
			XFillRectangle(xw.dpy, xw.win, gc, cells[i][j].x, cells[i][j].y,
				cell_size, cell_size);
		}
	}
	figure->Draw();

	XFreeGC(xw.dpy, gc);
	XFlush(xw.dpy);
}

ChessBoard::~ChessBoard() {
	for (int i = 0; i < cell_number; ++i) {
		delete[] cells[i];
	}
	delete[] cells;
}

void ChessBoard::initFigure(Figure* figure) { this->figure = figure; }
unsigned short ChessBoard::getCellSize() { return this->cell_size; }
unsigned short ChessBoard::getCellNumber() { return this->cell_number; }

