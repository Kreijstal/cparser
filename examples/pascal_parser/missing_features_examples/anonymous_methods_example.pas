program AnonymousMethodsExample;

{$mode objfpc}{$H+}

uses
  SysUtils;

type
  TProcOfInt = reference to procedure(AValue: Integer);

procedure ForEach(AValues: array of Integer; AProc: TProcOfInt);
var
  i: Integer;
begin
  for i in AValues do
    AProc(i);
end;

var
  MyArray: TArray<Integer>;
  Sum: Integer;

begin
  SetLength(MyArray, 5);
  MyArray[0] := 1;
  MyArray[1] := 2;
  MyArray[2] := 3;
  MyArray[3] := 4;
  MyArray[4] := 5;

  Sum := 0;
  ForEach(MyArray,
    procedure(AValue: Integer)
    begin
      Sum := Sum + AValue;
    end
  );

  Writeln('Sum: ', Sum);
end.
