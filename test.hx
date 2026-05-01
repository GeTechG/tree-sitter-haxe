package my.pkg;

import haxe.io.Bytes;
using StringTools;

typedef Point = { x: Float, y: Float };

enum Color {
  Red;
  Green;
  Blue;
  RGB(r: Int, g: Int, b: Int);
}

interface Drawable {
  function draw(): Void;
}

abstract Meters(Float) from Float to Float {
  inline public function new(v: Float) this = v;
  @:op(A + B) public inline function add(rhs: Meters): Meters return new Meters(this + rhs);
}

class Shape implements Drawable {
  public var name: String;
  static var count: Int = 0;

  public function new(name: String) {
    this.name = name;
    count++;
  }

  public function draw(): Void {
    trace('drawing $name');
  }

  public static function create(name: String): Shape {
    return new Shape(name);
  }
}

@:macro
class Macros {
  public static macro function build(): haxe.macro.Expr {
    return macro $v{42};
  }
}
