 /*
 * Oscilloscope
 * Gives a visual rendering of analog pins 0 and 1 in realtime.
 * 
 * (c) 2008 Sofian Audry (info@sofianaudry.com)
 * Original blog:   https://www.instructables.com/Arduino-Oscilloscope-poor-mans-Oscilloscope/
 * Original source: https://gist.github.com/chrismeyersfsu/3270419#file-gistfile1-c
 * (c) 2019 Zsolt Szilagyi (https://gist.github.com/zsolt-szilagyi)
 * Pause feature:   https://gist.github.com/zsolt-szilagyi/2e4088f6ba0d30c04aa6bc589fb41739
 * (c) 2024 Marc (hadoopmarc@xs4all.nl)
 * Current source:  https://github.com/hadoopmarc/gps-controlled-lcd-shutter
*
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
  
import processing.serial.*;

Serial port;         // Create object from Serial class
IntList vals;        // Interleaved data received from serial ports A0 & A1
int[] values0;
int[] values1;
float zoom;
boolean paused;
boolean encounter0;  // Poor man's positive edge triggering
boolean encounter1;
int xpause = 200;    // Position of pause trigger point

void setup() 
{
  size(1280, 600);
  // Open the port that the board is connected to and use the same baudrate as the Arduino
  port = new Serial(this, Serial.list()[1], 2000000);
  values0 = new int[width];
  values1 = new int[width];
  zoom = 1.0f;
  paused = false;
}

int getY(int val) {
  int padding = 10;
  return (int)(height - padding / 2 - val / 1023.0f * (height - padding)) ;
}

// Arduino UNO R3 takes about 8000 analog measurements per second.
// Processing IDE seems to have a minimum buffer size of 96 bytes for serial
// events to get processed, irrespective of the port.buffer(3) setting or the
// frameRate of the draw() loop. So, the implementation below reads all values
// from the buffer, once they are available.
IntList getValues() {
  IntList vals = new IntList();
  while (port.available() >= 5) {
    if (port.read() == 0xff) {
      int value = port.read() << 8 | port.read();
      vals.append(value);
      value = port.read() << 8 | port.read();
      vals.append(value);
    }
  }
  return vals;
}

void pushValues(IntList vals) {
  int nval = vals.size() / 2;   // values are interleaved for A0 and A1
  for (int i = 0; i < width; i++)
    if (i < width - nval) {
      values0[i] = values0[i + nval];
      values1[i] = values1[i + nval];
    } else {
      values0[i] = vals.get(2 * (i - width + nval));
      values1[i] = vals.get(2 * (i - width + nval) + 1);
    }
  if (paused && !encounter0 && (values0[xpause] < 150)) {
    encounter0 = true;
  } else if (paused && encounter0 && !encounter1 && (values0[xpause] > 150)) {
    encounter1 = true;
  }
}

void drawLines() {
  stroke(255);
  
  int displayWidth = (int) (width / zoom);
  
  int k = width - displayWidth;
  
  int xa = 0;
  int ya0 = getY(values0[k]);
  int ya1 = getY(values1[k]);
  for (int i=1; i<displayWidth; i++) {
    k++;
    int xb = (int) (i * (width-1) / (displayWidth-1));
    int yb0 = getY(values0[k]);
    int yb1 = getY(values1[k]);
    stroke(255, 255, 255);
    line(xa, ya0, xb, yb0);
    stroke(255, 253, 175);
    line(xa, ya1, xb, yb1);
    xa = xb;
    ya0 = yb0;
    ya1 = yb1;
  }
}

void drawGrid() {
  stroke(255, 0, 0);
  line(0, height/2, width, height/2);
}

void keyReleased() {
  switch (key) {
    case '+':
      zoom *= 2.0f;
      println(zoom);
      if ( (int) (width / zoom) <= 1 )
        zoom /= 2.0f;
      break;
    case '-':
      zoom /= 2.0f;
      if (zoom < 1.0f)
        zoom *= 2.0f;
      break;
    case 'p':
      paused = !paused;
      encounter0 = false;
      encounter1 = false;
      break;
    }
}

void draw()
{
  if (paused && encounter0 && encounter1) {
    return;
  }
  
  background(0);
  drawGrid();
  vals = getValues();
  if (vals.size() > 0) {
    pushValues(vals);
  }
  drawLines();
}
