program PropertiesForwardDeclExample;

{$mode objfpc}{$H+}

uses
  SysUtils;

type
  TClassB; // Forward declaration

  TClassA = class
  private
    FValue: Integer;
    FItems: array[0..9] of string;
    function GetValue: Integer;
    procedure SetValue(AValue: Integer);
    function GetItem(Index: Integer): string;
    procedure SetItem(Index: Integer; const AValue: string);
  public
    FClassB: TClassB;
    property Value: Integer read GetValue write SetValue;
    property Items[Index: Integer]: string read GetItem write SetItem;
    constructor Create;
    destructor Destroy; override;
  end;

  TClassB = class
  public
    FClassA: TClassA;
    constructor Create;
  end;

constructor TClassA.Create;
begin
  inherited Create;
  FClassB := TClassB.Create;
  FClassB.FClassA := Self;
end;

destructor TClassA.Destroy;
begin
  FClassB.Free;
  inherited Destroy;
end;

function TClassA.GetValue: Integer;
begin
  Result := FValue;
end;

procedure TClassA.SetValue(AValue: Integer);
begin
  FValue := AValue;
end;

function TClassA.GetItem(Index: Integer): string;
begin
  Result := FItems[Index];
end;

procedure TClassA.SetItem(Index: Integer; const AValue: string);
begin
  FItems[Index] := AValue;
end;

constructor TClassB.Create;
begin
  // FClassA will be set by TClassA's constructor
end;

var
  ObjA: TClassA;

begin
  ObjA := TClassA.Create;
  try
    ObjA.Value := 123;
    Writeln('ObjA.Value: ', ObjA.Value);

    ObjA.Items[0] := 'First';
    Writeln('ObjA.Items[0]: ', ObjA.Items[0]);

    if Assigned(ObjA.FClassB) and Assigned(ObjA.FClassB.FClassA) then
      Writeln('Forward declaration and mutual reference successful.');

  finally
    ObjA.Free;
  end;
end.
