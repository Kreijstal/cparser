program OperatorOverloadingExample;

{$mode objfpc}{$H+}

uses
  SysUtils;

type
  TVector = record
    X, Y: Double;
    class operator Add(a, b: TVector): TVector;
    function ToString: string;
  end;

class operator TVector.Add(a, b: TVector): TVector;
begin
  Result.X := a.X + b.X;
  Result.Y := a.Y + b.Y;
end;

function TVector.ToString: string;
begin
  Result := Format('(%f, %f)', [X, Y]);
end;

var
  V1, V2, V3: TVector;

begin
  V1.X := 1.0;
  V1.Y := 2.0;
  V2.X := 3.0;
  V2.Y := 4.0;

  V3 := V1 + V2;

  Writeln('V1: ', V1.ToString);
  Writeln('V2: ', V2.ToString);
  Writeln('V3 = V1 + V2: ', V3.ToString);
end.
