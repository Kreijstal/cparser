program GenericsExample;

{$mode objfpc}{$H+}

uses
  SysUtils;

type
  TList<T> = class
  private
    FItems: TArray<T>;
    FCount: Integer;
    function GetItem(Index: Integer): T;
  public
    procedure Add(const AItem: T);
    function Count: Integer;
    property Items[Index: Integer]: T read GetItem;
    function GetEnumerator: TArray<T>.TEnumerator;
  end;

procedure TList<T>.Add(const AItem: T);
begin
  SetLength(FItems, FCount + 1);
  FItems[FCount] := AItem;
  Inc(FCount);
end;

function TList<T>.Count: Integer;
begin
  Result := FCount;
end;

function TList<T>.GetItem(Index: Integer): T;
begin
  Result := FItems[Index];
end;

function TList<T>.GetEnumerator: TArray<T>.TEnumerator;
begin
  Result := System.TArray.GetEnumerator<T>(FItems);
end;

var
  MyList: TList<string>;
  s: string;

begin
  MyList := TList<string>.Create;
  try
    MyList.Add('Hello');
    MyList.Add('World');

    for s in MyList do
      Writeln(s);

  finally
    MyList.Free;
  end;
end.
