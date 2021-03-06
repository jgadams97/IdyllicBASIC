IdyllicBASIC is a variant of BASIC that encapsulates memory management. This means that Idyllic scripts can store its variables within external memory devices. This includes anything from an external RAM to even a hard disk drive or an SD card.

Here is IdyllicBASIC being ran on an ATMega328P microcontroller using a 23LC512 for external RAM.

![hi](https://i.imgur.com/pMSvOzO.jpg)

Both of these things make Idyllic ideal for low-memory applications. The ATMega328P microcontroller, for example, has 2 KB of RAM. Normally, this would be not be enough to run BASIC scripts, as you could only fit a few lines of code into RAM.

However, Idyllic can make use of external RAMs and program memory. The 23LC512, for example, can store 64kB of RAM, which can be used for variable storage with Idyllic. Idyllic stores variables using 32-bit pointers, so it can handle external memory devices up to 4.2 gigabytes. It can easily be recompiled with 64-bit pointers if need be, or 16-bit pointers if you do not need to address more than 2^16 bytes. Idyllic also stores these variables using AVL trees, so lookup times are fast.

The version in the photo is compiled wtih 16-bit pointers due to program memory limitations (not RAM) on the ATMega328p, but a larger microcontroller could handle larger pointer sizes assuming that you needed more memory. 

Variables in Idyllic can be declared with the DIM command.
```
dim myVar
dim $myVar
```
There are three main data types in Idyllic: 1. Numbers. 2. Strings. 3. Addresses.

Only numbers and strings can be directly manipulated by the user. Strings begin with a "$" character. Addresses begin "@". Numbers begin with nothing.

Values can be assigned to variables with the equal sign.
```
myVar = 5.2
$myVar = "Hello, World!"
```
You can also assign values on the same line you declare the variable.
```
dim myVar = 5.2
dim $myVar = "Hello, World!"
```
Strings can be provided a size when declaring them. In the case above, the string is not provided a size, so it defaults to the size of the "Hello, World!" string. 

Trying to store a string into a string variable larger than it will cause the string to expand in size.

```
dim $myVar = "Hello, "
$myVar = $myVar + "World!"
```

Strings can have a specific size specified in their declaration.

```
dim $myVar[10]
```

It is recommended to specify the size of the string as much as possible. If a string has to grow in size, it needs a new node to store that new data, which takes up more memory. When iterating through the string, it would need to iterate through the nodes, which is slower. Thus, declaring your string of the proper size initially rather than have it dynamically grow on its own both uses less RAM and gives better performance. 

You cannot "dim" an address. Addresses are created like so:

```
@myAddress
```
The address automatically gets assigned its value, which depends on where it occurs in the file.

Addresses can be jumped to using "goto @myAddress". This can be used for looping.
```
dim x = 0
@loop
	print x
	x = x + 1
if x < 10 then
	goto @loop
end
```
This will print numbers 0 through 9.

You can also use the "sub" command to create a subroutine. When the interpreter sees a subroutine, it will skip all code until it sees "end". You can then call the subroutine using "gosub" rather than "goto" which, just like "goto", will jump to the address of the subroutine, however, once it hits "end", it will return to where it was called from.

```
sub @mysub
	print "Subroutine called."
end

print 1
gosub @mysub
print 2
gosub @mysub
print 3
```

The IF command can be used for conditional statements. The block of code between IF and END will get executed only if the condition between IF and "then" is true.

The format of an IF command is:
```
if LHS comparator RHS then
	...
end
```
Valid comparators are:
```
>=	Greater than or equal to.
<=	Less than or equal to.
>	Greater than.
<	Less than.
!=	Not equal to.
```
Only "==" and "!=" are valid for string comparisons. String comparisons only apply to the first 8 characters of the string.

Some commands output values. These values can be redirected into a variable using the "=>" symbol.
```
dim $name
$read => $name
print "Hello, " + $name + "!"
```
Scripts in their own file can be ran using the "run" command.
```
run "fibonacci.ib"
```

For better performance, scripts can first be loaded into RAM before running them.
```
load "fibonacci.ib"
run
```


If a number variable is declared with a size, it will become an array.
```
dim x[10]
x[5] = 12
x[2] = 8
print x[5] + x[2]
```
Arrays cannot have a size of 0. 

Array indices can either be numbers or a variable.

```
;Declare the array.
dim myArray[10]

;Fill the array.
dim i = 1
@loop1
	myArray[i] = i * 2
	i = i + 1
if i <= 10 then
	goto @loop1
end

;Print out the numbers.
i = 1
@loop2
	print myArray[i]
	i = i + 1
if i <= 10 then
	goto @loop2
end
```
Arrays are 1-indexed. You can also see here that comments begin with a semicolon.

Assigning a number array a value is valid. The number will simply be placed at the first index of the array.

```
dim x[10]
x = 12
print x[1]
print x
```

Strings can also be referenced using an index. This will return that single character at the given index.

```
dim $str = "Hello!"
print $str[1]
```

You may also assign that character a new value. 

```
dim $str = "Hello!"
$str[2] = "n"
print $str
```

Subroutines can be recursive. 

```
dim factorial
dim solution
sub @factorial
        if base == 0 then
                base = 1
                solution = 1
        end
        if base == 1 then
                solution = solution * factorial
                factorial = factorial - 1
                if factorial > 0 then
                        gosub @factorial
                end
                base = 0
                factorial = solution
        end
end

factorial = 12
gosub @factorial
print factorial

factorial = 8
gosub @factorial
print factorial
```

