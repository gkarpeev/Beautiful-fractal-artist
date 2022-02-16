#include <SFML/Graphics.hpp>
#include <iostream>
#include <cmath>
#include <thread>

using namespace std;
using namespace sf;

struct point {
  double x, y;
  point operator+(point p) {
      return {x + p.x, y + p.y};
  }
  point operator-(point p) {
      return {x - p.x, y - p.y};
  }
  point operator*(double k) {
      return {x * k, y * k};
  }
};

using pixel = point;

struct complex {
  double x, y;
  complex operator+(complex a) {
      return {x + a.x, y + a.y};
  }
  complex operator+(double k) {
      return {x + k, y};
  }
  complex operator-(complex a) {
      return {x - a.x, y - a.y};
  }
  complex operator-(double k) {
      return {x - k, y};
  }
  complex operator*(complex a) {
      return {x * a.x - y * a.y, x * a.y + y * a.x};
  }
  complex operator*(double k) {
      return {x * k, y * k};
  }
  complex operator/(complex a) {
      return {(x * a.x + y * a.y) / (a.x * a.x + a.y * a.y), (y * a.x - x * a.y) / (a.x * a.x + a.y * a.y)};
  }
  complex operator/(double k) {
      return {x / k, y / k};
  }
  complex operator^(int p) {
      complex res(1, 0);
      complex a(x, y);
      while (p) {
          if (p & 1) {
              res = res * a;
          }
          a = a * a;
          p >>= 1;
      }
      return res;
  }
  double abs() {
      return sqrt(x * x + y * y);
  }
  double abs_sqr() {
      return x * x + y * y;
  }
  complex() : x(0), y(0) {}
  complex(double x, double y) : x(x), y(y) {}
  complex(point p) : x(p.x), y(p.y) {}
};

const int W = 1920, H = 1080;
const int MAX_ITERATIONS = 100;

const int THREADS = 6;

double scale = 300;
double scale_factor = 1.15;

Uint8 pixels[W * H * 4];

const int repeat_gradient = 15;
int gradient_size;

const Uint8* gradient;

pixel center = {W / 2, H / 2};

const double r2 = 7 * 7;

pixel get_pixel(pixel p) {
    return pixel{(p.x - center.x) / scale, (p.y - center.y) / scale};
}

auto start = chrono::steady_clock::now().time_since_epoch();

double get_time() {
    return chrono::duration_cast<chrono::nanoseconds>(chrono::steady_clock::now().time_since_epoch()- start).count() / 1000000000.0;
}

void draw_Julia(int i, const complex &c) {
    for (; i < W; i += THREADS) {
        for (int j = 0; j < H; ++j) {
            complex z(get_pixel({static_cast<double>(i), static_cast<double>(j)}));

            int it;
            for (it = 0; it < MAX_ITERATIONS && z.abs_sqr() < r2; ++it) {
                z = (z^2) + c;
            }
            double len = z.abs();
            double m = 10 + double(it) - log2(log2(len) / log2(r2)) + 1.5 * cos(3 * get_time());
            // gradient2:
//             double m = 10 + double(it) - log2(log2(len)/log2(r2));
            int gradient_id = int(m * repeat_gradient) * 4 % gradient_size;
            int id = 4 * (j * W + i);
            if (it == MAX_ITERATIONS) {
                pixels[id] = pixels[id + 1] = pixels[id + 2] = 0;
                pixels[id + 3] = 255;
            } else {
                pixels[id] = gradient[gradient_id], pixels[id + 1] = gradient[gradient_id + 1];
                pixels[id + 2] = gradient[gradient_id + 2], pixels[id + 3] = gradient[gradient_id + 3];
            }
        }
    }
}

void build_Julia(const complex &c) {
    vector<thread> t;
    for (int i = 0; i < THREADS; ++i) {
        t.emplace_back(draw_Julia, i, c);
    }
    for (thread& th : t) {
        th.join();
    }
}

enum mode
{
  DRAG,
  SELECT,
};

int main() {
    RenderWindow window(VideoMode(W, H), "SFML!", Style::Titlebar | Style::Close);
    Texture texture;
    texture.create(W, H);
    sf::Sprite sprite;
    sprite.setTexture(texture);

    Image gradientImage;
    if (!gradientImage.loadFromFile("./gradient.png")) {
        return 1;
    }
    gradient = gradientImage.getPixelsPtr();
    gradient_size = gradientImage.getSize().x * gradientImage.getSize().y * 4;
    bool mousePressed = false;
    pixel prev, last;
    mode mode = DRAG;

    while (window.isOpen())
    {
        Event event;
        while (window.pollEvent(event)) {
/*            sf::RectangleShape rectangle;
            rectangle.setSize(sf::Vector2f(100, 50));
            rectangle.setOutlineColor(sf::Color::Red);
            rectangle.setOutlineThickness(5);
            rectangle.setPosition(10, 20);
            window.draw(rectangle);
            window.display();*/
            if (event.type == Event::Closed) {
                window.close();
            } else if (event.type == Event::KeyPressed) {
                if (event.key.code == Keyboard::Escape) {
                    window.close();
                } else if (!mousePressed) {
                    if (event.key.code == Keyboard::D) {
                        mode = DRAG;
                    } else if (event.key.code == Keyboard::S) {
                        mode = SELECT;
                    }
                }
            } else if (event.type == Event::MouseButtonPressed) {
                if (event.mouseButton.button == Mouse::Left) {
                    mousePressed = true;
                    prev.x = event.mouseButton.x;
                    prev.y = event.mouseButton.y;
                    last = prev;
                }

            } else if (event.type == Event::MouseButtonReleased) {
                if (event.mouseButton.button == Mouse::Left) {
                    if (mode == SELECT) {
                        double left = min(prev.x, last.x);
                        double right = max(prev.x, last.x);
                        double top = min(prev.y, last.y);
                        double bottom = max(prev.y, last.y);

                        pixel c{(left + right) / 2.0, (top + bottom) / 2.0};

                        center = (center - c) * (double(W) / (right - left));
                        scale *= double(W) / (right - left);
                        center = center + point{W / 2, H / 2};
                    }
                    mousePressed = false;
                }
            } else if (event.type == sf::Event::MouseMoved) {
                if (mousePressed) {
                    double x = event.mouseMove.x;
                    double y = event.mouseMove.y;
                    if (mode == DRAG) {
                        center.x += x - prev.x;
                        center.y += y - prev.y;
                        prev.x = x;
                        prev.y = y;
                    } else if (mode == SELECT) {
                        last.x = x;
                        last.y = y;
                    }
                }
            } else if (event.type == Event::MouseWheelScrolled) {
                double x = event.mouseWheelScroll.x;
                double y = event.mouseWheelScroll.y;
                if (event.mouseWheelScroll.delta == 1) {
                    center.x = x + (center.x - x) * scale_factor;
                    center.y = y + (center.y - y) * scale_factor;
                    scale *= scale_factor;
                } else {
                    center.x = x + (center.x - x) / scale_factor;
                    center.y = y + (center.y - y) / scale_factor;
                    scale /= scale_factor;
                }
            }
        }

        // for gradient2.png
//        build_Julia({0.285, 0});
        // for gradient.png
        build_Julia({-0.70176, -0.3842});
        texture.update(pixels);
        window.clear();
        window.draw(sprite);
        window.display();
    }

    return 0;
}
