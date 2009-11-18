// $Id$
// This file is part of SmallBASIC
//
// Copyright(C) 2001-2010 Chris Warren-Smith. [http://tinyurl.com/ja2ss]
//
// This program is distributed under the terms of the GPL v2.0 or later
// Download the GNU Public License (GPL) from www.gnu.org
//

#include <fltk/damage.h>
#include <fltk/events.h>
#include <fltk/CheckButton.h>

#include "TtyWidget.h"

#define BASE_FONT_SIZE 12
#define BASE_FONT COURIER

//
// TextSeg - a segment of text between escape chars
//
void TextSeg::setfont(bool* bold, bool* italic, bool* underline, bool* invert) {
  // update state variables when explicitely set in this segment
  *bold = get(BOLD, bold);
  *italic = get(ITALIC, italic);
  *underline = get(UNDERLINE, underline);
  *invert = get(INVERT, invert);

  Font* font = BASE_FONT;
  if (*bold) {
    font = font->bold();
  }
  if (*italic) {
    font = font->italic();
  }
  fltk::setfont(font, BASE_FONT_SIZE);

  if (this->color != NO_COLOR) {
    fltk::setcolor(this->color);
  }
}

//
// scrollbar callback to redraw upon scroll changes
//
void scrollbar_callback(Widget* scrollBar, void* ttyWidget) {
  ((TtyWidget *) ttyWidget)->redraw(DAMAGE_ALL);
}

//
// TtyWidget constructor
//
TtyWidget::TtyWidget(int x, int y, int w, int h, int numRows) :
  Group(x, y, w, h, 0) {

  // initialize the buffer
  buffer = new Row[numRows];
  rows = numRows;
  cols = width = 0;
  head = tail = 0;
  cursor = 0;
  markX = markY = pointX = pointY = 0;

  setfont(BASE_FONT, BASE_FONT_SIZE);
  lineHeight = (int) (getascent() + getdescent());

  begin();
  // vertical scrollbar scrolls in row units
  vscrollbar = new Scrollbar(w - SCROLL_W, 1, SCROLL_W, h);
  vscrollbar->set_vertical();
  vscrollbar->user_data(this);
  vscrollbar->callback(scrollbar_callback);

  // horizontal scrollbar scrolls in pixel units
  hscrollbar = new Scrollbar(w - HSCROLL_W - SCROLL_W, 1, HSCROLL_W, SCROLL_H);
  vscrollbar->user_data(this);
  vscrollbar->callback(scrollbar_callback);

  end();
}

TtyWidget::~TtyWidget() {
  delete[] buffer;
}

//
// draw the text
//
void TtyWidget::draw() {
  // get the text drawing rectangle
  Rectangle rc = Rectangle(0, 0, w(), h()+1);
  if (vscrollbar->visible()) {
    rc.move_r(-vscrollbar->w());
  }

  // prepare escape state variables
  bool bold = false;
  bool italic = false;
  bool underline = false;
  bool invert = false;

  // calculate rows to display
  int pageRows = getPageRows();
  int textRows = getTextRows();
  int vscroll = vscrollbar->value();
  int hscroll = hscrollbar->value();
  int numRows = textRows < pageRows ? textRows : pageRows;
  int firstRow = tail + vscroll;

  // setup the background colour
  setcolor(color());
  fillrect(rc);
  push_clip(rc);
  setcolor(BLACK);
  drawline(0, 0, w(), 0);
  setcolor(labelcolor());

  int pageWidth = 0;
  for (int row = firstRow, rows = 0, y = rc.y() + lineHeight;
       rows < numRows; row++, rows++, y += lineHeight) {
    Row* line = getLine(row); // next logical row
    TextSeg* seg = line->head;
    int x = 2 - hscroll;
    while (seg != NULL) {
      seg->setfont(&bold, &italic, &underline, &invert);
      drawSelection(seg, null, row, x, y);
      int width = seg->width();
      if (seg->str) {
        if (invert) {
          setcolor(labelcolor());
          fillrect(x, y, width, lineHeight);
          setcolor(color());
        }
        drawtext(seg->str, x, y);
      }
      if (underline) {
        int ascent = (int)getascent();
        drawline(x, y+ascent+1, x+width, y+ascent+1);
      }
      x += width;
      seg = seg->next;
    }
    int rowWidth = line->width();
    if (rowWidth > pageWidth) {
      pageWidth = rowWidth;
    }
  }

  // draw scrollbar controls
  if (pageWidth > w()) {
    draw_child(*hscrollbar);
  }
  pop_clip();
  draw_child(*vscrollbar);
}

//
// draw the background for selected text
//
void TtyWidget::drawSelection(TextSeg* seg, String* s, int row, int x, int y) {
  if (markX != pointX || markY != pointY) {
    Rectangle rc(0, y - getascent(), 0, lineHeight);
    int r1 = markY;
    int r2 = pointY;
    int x1 = markX; 
    int x2 = pointX; 

    if (r1 > r2) {
      r1 = pointY;
      r2 = markY;
      x1 = pointX;
      x2 = markX;
    }
    if (r1 == r2 && x1 > x2) {
      x1 = pointX;
      x2 = markX;
    }

    if (row > r1 && row < r2) {
      // entire row
      rc.x(x);
      rc.w(seg->width());
      if (s) {
        s->append(seg->str);
      }
    }
    else if (row == r1 && (r2 > r1 || x < x2)) {
      // top selection row
      int i = 0;
      int len = seg->numChars();

      // find start of selection
      while (x < x1 && i < len) {
        x += getwidth(seg->str + (i++), 1);
      }
      rc.x(x);

      // select rest of line when r2>r1
      while ((r2 > r1 || x < x2) && i < len) {
        if (s) {
          s->append(seg->str[i]);
        }
        x += getwidth(seg->str + (i++), 1);
      }
      rc.set_r(x);
    }
    else if (row == r2) {
      // bottom selection row
      rc.x(x);

      // select rest of line when r2>r1
      int i = 0;
      int len = seg->numChars();
      while (x < x2 && i < len) {
        if (s) {
          s->append(seg->str[i]);
        }
        x += getwidth(seg->str + (i++), 1);
      }
      rc.set_r(x);
    }

    if (!s && !rc.empty()) {
      setcolor(YELLOW);
      fillrect(rc);
      setcolor(labelcolor());
    }
  }
}

//
// process mouse messages
//
int TtyWidget::handle(int e) {
  switch (e) {
  case PUSH:
    if (get_key_state(LeftButton) && 
        (!vscrollbar->visible() || !event_inside(*vscrollbar))) {
      markX = pointX = event_x();
      markY = pointY = (event_y() / lineHeight) + vscrollbar->value();
      redraw(DAMAGE_HIGHLIGHT);
    }
    break;

  case MOVE:
    if (get_key_state(LeftButton)) {
      pointX = event_x();
      pointY = (event_y() / lineHeight) + vscrollbar->value();
      redraw(DAMAGE_HIGHLIGHT);
    }
    return 1;
  }

  vscrollbar->handle(e);
  return Group::handle(e);
}

//
// update scrollbar positions
//
void TtyWidget::layout() {
  int pageRows = getPageRows();
  int textRows = getTextRows();
  int hscrollX = w() - HSCROLL_W;
  int hscrollW = w() - 4;

  if (textRows > pageRows && h() > SCROLL_W) {
    vscrollbar->set_visible();
    vscrollbar->value(vscrollbar->value(), pageRows, 0, textRows);
    vscrollbar->resize(w() - SCROLL_W, 1, SCROLL_W, h());
    hscrollX -= SCROLL_W;
    hscrollW -= SCROLL_W;
  }
  else {
    vscrollbar->clear_visible();
    vscrollbar->value(0);
  }

  if (width > hscrollW) {
    hscrollbar->set_visible();
    hscrollbar->value(hscrollbar->value(), hscrollW, 0, width);
    hscrollbar->resize(hscrollX, 1, HSCROLL_W, SCROLL_H);
  }
  else {
    hscrollbar->clear_visible();
    hscrollbar->value(0);
  }
}

//
// copy selected text to the clipboard
//
bool TtyWidget::copySelection() {
  int hscroll = hscrollbar->value();
  bool bold = false;
  bool italic = false;
  bool underline = false;
  bool invert = false;
  int r1 = markY;
  int r2 = pointY;

  if (r1 > r2) {
    r1 = pointY;
    r2 = markY;
  }

  String selection;

  for (int row = r1; row <= r2; row++) {
    Row* line = getLine(row); // next logical row
    TextSeg* seg = line->head;
    int x = 2 - hscroll;
    String rowText;
    while (seg != NULL) {
      seg->setfont(&bold, &italic, &underline, &invert);
      drawSelection(seg, &rowText, row, x, 0);
      x += seg->width();
      seg = seg->next;
    }
    if (rowText.length()) {
      selection.append(rowText);
      selection.append("\n");
    }
  }

  bool result = selection.length() > 0;
  if (result) {
    const char *copy = selection.toString();
    fltk::copy(copy, strlen(copy), 0);
  }
  return result;
}

//
// clear the screen
//
void TtyWidget::clearScreen() {

}

//
// process incoming text
//
void TtyWidget::print(const char *str) {
  int strLength = strlen(str);
  Row* line = getLine(head); // pointer to current line

  // need the current font set to calculate text widths
  fltk::setfont(BASE_FONT, BASE_FONT_SIZE);

  // scan the text, handle any special characters, and display the rest.
  for (int i = 0; i < strLength; i++) {
    // check for telnet IAC codes
    switch (str[i]) {
    case '\r': // return
      // move to the start of the line
      cursor = 0;
      break;

    case '\a':
      // beep!
      break;

    case '\n': // new line
      // scroll by moving logical last line
      head = (head + 1 >= rows) ? 0 : head + 1;
      if (head == tail) {
        tail++;
      }

      // clear the new line
      line = getLine(head);
      line->clear();
      break;

    case '\b': // backspace
      // move back one space
      if (cursor > 0) {
        redraw(DAMAGE_ALL);
      }
      break;

    case '\t':
      cursor += 8;
      break;

    default:
      i += processLine(line, &str[i]);
    } // end case
  }

  vscrollbar->value(getTextRows() - getPageRows());
}

//
// return a pointer to the specified line of the display.
//
Row* TtyWidget::getLine(int pos) {
  if (pos < 0) {
    pos += rows;
  }
  if (pos > rows - 1) {
    pos -= rows;
  }

  return &buffer[pos];
}

//
// interpret ANSI escape codes in linePtr and return number of chars consumed
//
int TtyWidget::processLine(Row* line, const char* linePtr) {
  const char* linePtrNext = linePtr;

  // Walk the line of text until an escape char or end-of-line
  // is encountered.
  while (*linePtr > 31) {
    linePtr++;
  }

  // Print the next (possible) line of text
  if (*linePtrNext != '\0') {
    TextSeg* segment = new TextSeg(linePtrNext, (linePtr - linePtrNext));
    line->append(segment);

    // save max rows encountered
    int lineWidth = line->width();
    if (lineWidth > width) {
      width = lineWidth;
    }

    int lineChars = line->numChars();
    if (lineChars > cols) {
      cols = lineChars;
    }

    // Determine if we are at an end-of-line or an escape
    bool escaped = false;
    if (*linePtr == '\033') {
      linePtr++;

      if (*linePtr == '[') {
        escaped = true;
        linePtr++;
      }

      if (escaped) {
        int param = 0;
        while (*linePtr != '\0' && escaped) {
          // Walk the escape sequence
          switch (*linePtr) {
          case 'm':
            escaped = false;  // fall through

          case ';':  // Parameter seperator
            setGraphicsRendition(segment, param);
            param = 0;
            break;

          case '0': case '1': case '2': case '3': case '4':
          case '5': case '6': case '7': case '8': case '9':
            // Numeric OK; continue till delimeter or illegal char
            param = (param * 10) + (*linePtr - '0');
            break;

          default: // Illegal character - reset
            segment->flags = 0;
            escaped = false;
          }
          linePtr += 1;
        } // while escaped and not null
      }
    }
    else {
      // next special char was not escape
      linePtr--;
    }
  }
  return (linePtr - linePtrNext);
}

//
// performs the ANSI text SGI function.
//
void TtyWidget::setGraphicsRendition(TextSeg* segment, int c) {
  switch (c) {
  case 1:  // Bold on
    segment->set(TextSeg::BOLD, true);
    break;

  case 2:  // Faint on
    segment->set(TextSeg::BOLD, false);
    break;

  case 3:  // Italic on
    segment->set(TextSeg::ITALIC, true);
    break;

  case 4:  // Underscrore
    segment->set(TextSeg::UNDERLINE, true);
    break;

  case 7: // reverse video on
    segment->set(TextSeg::INVERT, true);
    break;

  case 21: // set bold off
    segment->set(TextSeg::BOLD, false);
    break;

  case 23:
    segment->set(TextSeg::ITALIC, false);
    break;

  case 24: // set underline off
    segment->set(TextSeg::UNDERLINE, false);
    break;

  case 27: // reverse video off
    segment->set(TextSeg::INVERT, false);
    break;

  case 30: // Black
    segment->color = BLACK;
    break;

  case 31: // Red
    segment->color = RED;
    break;

  case 32: // Green
    segment->color = GREEN;
    break;

  case 33: // Yellow
    segment->color = YELLOW;
    break;

  case 34: // Blue
    segment->color = BLUE;
    break;

  case 35: // Magenta
    segment->color = MAGENTA;
    break;

  case 36: // Cyan
    segment->color = CYAN;
    break;

  case 37: // White
    segment->color = WHITE;
    break;
  }
}

// End of "$Id$".
