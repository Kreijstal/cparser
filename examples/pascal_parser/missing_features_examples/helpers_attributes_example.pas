program HelpersAttributesExample;

{$mode objfpc}{$H+}

uses
  SysUtils;

type
  MyAttribute = class(TCustomAttribute)
  private
    FDescription: string;
  public
    constructor Create(const ADescription: string);
    property Description: string read FDescription;
  end;

  [MyAttribute('This is a custom attribute for a class')]
  TMyClass = class
  public
    procedure DoSomething;
  end;

  TMyClassHelper = class helper for TMyClass
  public
    procedure DoSomethingElse;
  end;

  TIntegerHelper = record helper for Integer
  public
    function IsEven: Boolean;
  end;

constructor MyAttribute.Create(const ADescription: string);
begin
  FDescription := ADescription;
end;

procedure TMyClass.DoSomething;
begin
  Writeln('TMyClass.DoSomething');
end;

procedure TMyClassHelper.DoSomethingElse;
begin
  Writeln('TMyClassHelper.DoSomethingElse');
end;

function TIntegerHelper.IsEven: Boolean;
begin
  Result := Self mod 2 = 0;
end;

var
  MyObject: TMyClass;
  MyInt: Integer;

begin
  MyObject := TMyClass.Create;
  try
    MyObject.DoSomething;
    MyObject.DoSomethingElse;
  finally
    MyObject.Free;
  end;

  MyInt := 42;
  Writeln(MyInt, ' is even: ', MyInt.IsEven);
  MyInt := 43;
  Writeln(MyInt, ' is even: ', MyInt.IsEven);
end.
