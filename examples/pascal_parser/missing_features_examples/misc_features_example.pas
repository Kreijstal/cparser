program MiscFeaturesExample;

{$mode objfpc}{$H+}

uses
  SysUtils;

type
  TPoint = record
    X, Y: Integer;
  end;

const
  MyPoint: TPoint = (X: 10; Y: 20);
  MyArray: array[0..2] of string = ('One', 'Two', 'Three');

procedure GetCoordinates(out AX, AY: Integer);
begin
  AX := MyPoint.X;
  AY := MyPoint.Y;
end;

var
  X, Y: Integer;
  i: Integer;

begin
  Writeln('Typed constant record: (', MyPoint.X, ', ', MyPoint.Y, ')');

  for i := Low(MyArray) to High(MyArray) do
    Writeln('Typed constant array[', i, ']: ', MyArray[i]);

  GetCoordinates(X, Y);
  Writeln('Coordinates from out parameters: (', X, ', ', Y, ')');
end.
