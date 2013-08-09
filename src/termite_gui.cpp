/* Copyright 2013 Little IO
 *
 * termite is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * termite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with termite.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "termite_gui.h"

#include "value.h"
#include "termite_synth.h"

#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <sstream>
#include <unistd.h>

#define LOGO_WIDTH 36
#define LOGO_Y 0
#define START_X 20
#define START_Y 7
#define SPACE 3
#define SECTION_Y 7
#define MAX_STATUS_SIZE 5

namespace {
  int getWidth() {
    int xdim;
    int ydim;
    getmaxyx(stdscr, ydim, xdim);
    return xdim;
  }
}

namespace laf {

  void TermiteGui::drawLogo() {
    attron(A_BOLD);
    attron(COLOR_PAIR(LOGO_COLOR));
    int logo_x = (getWidth() - LOGO_WIDTH) / 2;

    move(LOGO_Y, logo_x);
    printw("   __                        __");
    move(LOGO_Y + 1, logo_x);
    printw("  / /____  _________ ___ (--) /____");
    move(LOGO_Y + 2, logo_x);
    printw(" / __/ _ \\/ ___/ __ `__ \\/ / __/ _ \\");
    move(LOGO_Y + 3, logo_x);
    printw("/ /_/  __/ /  / / / / / / / /_/  __/");
    move(LOGO_Y + 4, logo_x);
    printw("\\__/\\___/_/  /_/ /_/ /_/_/\\__/\\___/");

    attroff(A_BOLD);
    attroff(COLOR_PAIR(LOGO_COLOR));

    move(LOGO_Y + 5, logo_x);
    printw("                          Little IO");
  }

  void TermiteGui::drawMidi(std::string status) {
    move(1,0);
    printw("MIDI Learn: ");
    attron(A_BOLD);
    hline(' ', MAX_STATUS_SIZE);
    printw(status.substr(0, MAX_STATUS_SIZE).c_str());
    attroff(A_BOLD);
    refresh();
  }

  void TermiteGui::drawStatus(std::string status) {
    move(0,0);
    printw("Current Value: ");
    attron(A_BOLD);
    hline(' ', MAX_STATUS_SIZE);
    printw(status.substr(0, MAX_STATUS_SIZE).c_str());
    attroff(A_BOLD);
    refresh();
  }

  void TermiteGui::drawControl(const Control* control, bool active) {
    Slider* slider = slider_lookup_[control];
    char slider_char = active ? '=' : ' ';
    if (active)
      attron(A_BOLD);
    move(slider->y, slider->x);
    printw(slider->label.c_str());
    attroff(A_BOLD);

    // Clear slider.
    move(slider->y + 1, slider->x);
    attron(COLOR_PAIR(SLIDER_BG_COLOR));
    hline(slider_char, slider->width);
    attroff(COLOR_PAIR(SLIDER_BG_COLOR));

    // Find slider position.
    laf_sample relative_value = control->current_value - control->min;
    laf_sample relative_width = control->max - control->min;
    int position = round(slider->width * relative_value / relative_width);
    int slider_midpoint = (slider->width + 1) / 2;

    attron(COLOR_PAIR(SLIDER_FG_COLOR));
    if (slider->bipolar) {
      if (position < slider_midpoint) {
        move(slider->y + 1, slider->x + position);
        hline(' ', slider_midpoint - position);
      }
      else {
        move(slider->y + 1, slider->x + slider_midpoint);
        hline(' ', position - slider_midpoint);
      }
    }
    else {
      hline(' ', position);
    }

    attroff(COLOR_PAIR(SLIDER_FG_COLOR));
    refresh();
  }

  void TermiteGui::drawControlStatus(const Control* control, bool midi_armed) {
    std::ostringstream midi_learn;
    if (midi_armed)
      midi_learn << "ARMED";
    else if (control->midi_learn)
      midi_learn << control->midi_learn;
    else
      midi_learn << "-";
    drawMidi(midi_learn.str());

    std::ostringstream status;
    status << control->current_value;
    drawStatus(status.str());
  }

  void TermiteGui::start() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);

    if(has_colors() == FALSE) {
      endwin();
      printf("Your terminal does not support color\n");
      return;
    }

    start_color();
    init_pair(SLIDER_FG_COLOR, COLOR_WHITE, COLOR_YELLOW);
    init_pair(SLIDER_BG_COLOR, COLOR_YELLOW, COLOR_WHITE);
    init_pair(LOGO_COLOR, COLOR_RED, COLOR_BLACK);

    drawLogo();

    curs_set(0);
  }

  void TermiteGui::stop() {
    endwin();
  }

  void TermiteGui::addPerformanceControls(const ControlGroup* performance) {
    std::map<std::string, Control*>::const_iterator iter =
        performance->controls.begin();
    int section_width = (getWidth() - 4 * SPACE) / 3;
    for (int i = 0; iter != performance->controls.end(); ++iter, ++i) {
      Slider* slider = new Slider();
      slider->x = SPACE;
      slider->y = SECTION_Y + 20 + SPACE * i;
      slider->width = section_width;
      slider->label = iter->first;

      slider_lookup_[iter->second] = slider;
      drawControl(iter->second, false);
    }
  }

  void TermiteGui::addOscillatorControls(const ControlGroup* oscillator) {
    std::map<std::string, Control*>::const_iterator iter =
        oscillator->controls.begin();
    int section_width = (getWidth() - 4 * SPACE) / 3;
    for (int i = 0; iter != oscillator->controls.end(); ++iter, ++i) {
      Slider* slider = new Slider();
      slider->x = SPACE;
      slider->y = SECTION_Y + SPACE * i;
      slider->width = section_width;
      slider->label = iter->first;
      slider->bipolar = iter->second->min < 0;

      slider_lookup_[iter->second] = slider;
      drawControl(iter->second, false);
    }
  }

  void TermiteGui::addFilterControls(const ControlGroup* filter) {
    std::map<std::string, Control*>::const_iterator iter =
        filter->controls.begin();
    int section_width = (getWidth() - 4 * SPACE) / 3;
    for (int i = 0; iter != filter->controls.end(); ++iter, ++i) {
      Slider* slider = new Slider();
      slider->x = section_width + SPACE * 2;
      slider->y = SECTION_Y + SPACE * i;
      slider->width = section_width;
      slider->label = iter->first;
      slider->bipolar = iter->second->min < 0;

      slider_lookup_[iter->second] = slider;
      drawControl(iter->second, false);
    }
  }

  void TermiteGui::addAmplifierControls(const ControlGroup* amplifier) {
    std::map<std::string, Control*>::const_iterator iter =
        amplifier->controls.begin();
    int section_width = (getWidth() - 4 * SPACE) / 3;
    for (int i = 0; iter != amplifier->controls.end(); ++iter, ++i) {
      Slider* slider = new Slider();
      slider->x = section_width * 2 + SPACE * 3;
      slider->y = SECTION_Y + SPACE * i;
      slider->width = section_width;
      slider->label = iter->first;
      slider->bipolar = iter->second->min < 0;

      slider_lookup_[iter->second] = slider;
      drawControl(iter->second, false);
    }
  }
} // namespace laf
