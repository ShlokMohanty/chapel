var myDom: domain(1) = {0..0};
var myArray: [myDom][1..2] uint(8);

proc main() {
  myDom = {0..20};
  dump();
}

proc dump() {
  writeln("myDom is: ", myDom);
  for i in myDom {
    write("(", myArray[i].domain, "): ");
    for j in myArray[i].domain {
      write(myArray[i][j], " ");
    }
    writeln();
  }
}
