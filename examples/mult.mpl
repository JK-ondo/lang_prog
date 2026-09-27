program sample;
var n, mult, data : integer;
begin
	writeln('input the number of data');
	readln(n);
	mult := 0;
	while n > 0 do begin
		readln(data);
		mult := mult * data;
		n := n - 1
	end;
	writeln('Multplex of data = ', mult)
end.
