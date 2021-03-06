
#include "parsing/evaluator_dynamic.h"
char evalPos(Evaluator *e, ibword pos);
ibword PROGRAM_LOAD_ADDR;

#ifdef ARDUINO
	#include "edit.c"
#endif

//Returns -1 if line is too long.
//	Otherwise returns offset.
char copyStringIntoLineBuff(Evaluator *e, char *s) {
	ibword i = 0;
	while (!isEOL(s[i])) {
		e->line_buff[i] = s[i];
		i++;
		if (i == LINE_BUFF_MAX)
			break;
	}
	if (i == LINE_BUFF_MAX)
		return false;
	e->line_buff[i] = 0;
	return true;
}

ibword copyFileIntoLineBuff(Evaluator *e, ibword pos) {
	ibword i = 0;
	char c = e->in_ram ? readRAM(pos) : readFileOnDevice(pos);
	while (!isEOL(c)) {
		e->line_buff[i++] = c;
		pos += 1;
		if (i == LINE_BUFF_MAX)
			break;
		c = e->in_ram ? readRAM(pos) : readFileOnDevice(pos);
	}
	if (i == LINE_BUFF_MAX)
		return -1;
	e->line_buff[i] = 0;
	return i + 1;
}

//Evaluate a reference statement.
//	0	No Error.
char evalReference(Evaluator *e, ibword addr) {
	ibword pos = 0;
	char c;
	char key[KEY_SIZE + 1];
	unsigned char p = 0;

	//Skip over garbage whitespace.
	c = e->line_buff[pos];
	while (isWS(c)) {
		c = e->line_buff[++pos];
	}

	//Load the key.
	while (!isWS(c) && !isEOL(c)) {
		key[p++] = c;
		c = e->line_buff[++pos];
		if (p > KEY_SIZE)
			return ERROR_INVALID_KEY;
	}
	key[p] = 0;

	//Check if there is more garbage text afterwards.
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}
	if (!isEOL(c))
		return ERROR_SYNTAX;
	if (!verifyKey(key))
		return ERROR_INVALID_KEY;

	//Check if the key exists.
	ibword addr2 = findNode(key);
	if (addr2 != (ibword)undefined) {
		//Return an error if we're trying to
		//	change the key's address.
		if (readAdr(addr2) != addr)
			return ERROR_KEY_EXISTS;
		return 0;
	}
	//No issues. Create the key.
	dimAdr(key, addr);

	return 0;
}


char evalSubroutine(Evaluator *e, ibword addr) {
	char c = 0;
	ibword pos = 0;

	//Skip over garbage whitespace.
	c = e->line_buff[pos];
	while (isWS(c)) {
		c = e->line_buff[++pos];
	}

	//Remove "sub".
	c = e->line_buff[pos];
	if (c != 'S' && c != 's')
		return ERROR_SYNTAX;
	e->line_buff[pos] = ' ';
	c = e->line_buff[++pos];
	if (c != 'U' && c != 'u')
		return ERROR_SYNTAX;
	e->line_buff[pos] = ' ';
	c = e->line_buff[++pos];
	if (c != 'B' && c != 'b')
		return ERROR_SYNTAX;
	e->line_buff[pos] = ' ';
	if (isWS(c))
		return ERROR_SYNTAX;

	char err = evalReference(e, addr);
	if (err != 0) return err;
	else return ERROR_SUBROUTINE;
}

//Evaluate an assignment.
//	0	No error.
//	If we are assigning a string that has yet
//		to have a size, we can pass the string
//		name into "newstr" and it will set the
//		size before defining it.
char evalAssignment(Evaluator *e, char *newstr) {

	char c;
	char key[KEY_SIZE];
	unsigned char keyPos = 0;
	ibword pos = 0;

	//Skip over white space.
	c = e->line_buff[pos];
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}

	//Syntax error.
	if (isEOL(c)) return 1;

	//Read in key.
	if (c == '$') {
		key[keyPos++] = c;
		c = e->line_buff[++pos];
	}
	while (isAlphaNum(c) && !isEOL(c)) {
		key[keyPos++] = c;
		if (keyPos > KEY_SIZE)
			return ERROR_INVALID_KEY;
		c = e->line_buff[++pos];
	}
	key[keyPos] = 0;

	//Syntax error.
	if (isEOL(c)) return ERROR_SYNTAX;

	//Key doesn't exist.
	if (!verifyKey(key)) return ERROR_INVALID_KEY;

	//Check if key exists.
	ibword addr;
	if (newstr == NULL)
		addr = findNode(key);
	else
		addr = undefined;
	if (addr == (ibword)undefined && newstr == NULL)
		return ERROR_KEY_NOT_FOUND;

	//Skip to equal sign.
	char isArray = 0;
	while (c != '=' && !isEOL(c)) {
		if (c == '[') {
			isArray = 1;
			break;
		}
		c = e->line_buff[++pos];
	}

	//Fetch array index.
	ibword index;
	if (isArray) {
		char numBuff[11];
		unsigned char numBuffPos = 0;
		char numIsVar = 0;
		char isOnlyNum = 1;
		c = e->line_buff[++pos];
		while (c != ']') {
			if (isEOL(c))
				return ERROR_SYNTAX;
			if (isAlpha(c))
				numIsVar = 1;
			if (!isNum(c) && !isWS(c))
				isOnlyNum = 0;
			if (!isWS(c))
				numBuff[numBuffPos++] = c;
			c = e->line_buff[++pos];
		}
		if (numBuffPos == 0)
			return ERROR_SYNTAX;
		numBuff[numBuffPos] = 0;
		if (numIsVar) {
			if (!verifyKey(numBuff))
				return ERROR_SYNTAX;
			ibword addrIndex = findNode(numBuff);
			if (addrIndex == (ibword)undefined)
				return ERROR_KEY_NOT_FOUND;
			index = (int)readNum(addrIndex);
		} else {
			if (isOnlyNum)
				index = atoi(numBuff);
			else
				return ERROR_SYNTAX;
		}
		index--;
		//Skip to equal sign.
		while (c != '=' && !isEOL(c)) {
			c = e->line_buff[++pos];
		}
	}

	//Syntax error.
	if (isEOL(c))
		return ERROR_SYNTAX;

	//Find start and size.
	c = e->line_buff[++pos];
	ibword start = pos;
	ibword size = 0;
	while (!isEOL(c)) {
		c = e->line_buff[++pos];
		size++;
	}

	//Copy formula to eval buff.
	if (isAlpha(key[0])) {
		if (!verifyFormula(e, start, size))
			return ERROR_SYNTAX;
		char err = copyFormulaIntoEvalBuff(e, start, size);
		if (err != 0) return err;
		if (isArray) {
			if (index < 0 || index >= readArrSize(addr))
				return ERROR_OUT_OF_BOUNDS;
			writeArr(addr, index, evaluateFormula(e));
		} else
			writeNum(addr, evaluateFormula(e));
	} else if (key[0] == '$') {
		//Make sure the string to evaluate is syntactically correct.
		if (!verifyString(e, start, size))
			return ERROR_SYNTAX;
		//Load the string for evaluation.
		char err = copyStringIntoEvalBuff(e, start, size);
		if (err != 0) return err;
		//Fetch the size of the string variable to store.
		ibword strSize;
		if (newstr == NULL)
			strSize = readStrSize(addr);
		else
			strSize = STRING_SIZE_MAX;
		//Iterator.
		ibword strPos = 0;
		char c = readCharFromEvalBuff(e);
		if (isArray) {
			if (index < 0 || index >= readStrSize(addr))
				return ERROR_OUT_OF_BOUNDS;
			writeStr(addr, index, c);
			//Reset the buffer.
			e->addr = undefined;
			e->str_pos = 0;
			readCharFromEvalBuff(e);
		} else {
			writeRAM(strPos + e->avl_end + sizeof(AVL_Node), c);
			strPos++;
			while ( (c = readCharFromEvalBuff(e)) ) {
				writeRAM(strPos + e->avl_end + sizeof(AVL_Node), c);
				strPos++;
			}
			if (strPos > strSize && newstr == NULL) {
				//Find next node.
				AVL_Node n = fetchNode(addr);
				while (n.next != (ibword)undefined) {
					n = fetchNode(n.next);
				}
				//Insert new dandling node.
				n.next = insertDanglingNode(strPos - strSize);
				storeNode(&n);
				//Recalculate string.
				strPos = 0;
				c = readCharFromEvalBuff(e);
				writeRAM(strPos + e->avl_end + sizeof(AVL_Node), c);
				strPos++;
				while ( (c = readCharFromEvalBuff(e)) ) {
					writeRAM(strPos + e->avl_end + sizeof(AVL_Node), c);
					strPos++;
				}
				strSize = strPos;
			}
			//Dim the string if it's specified to.
			if (newstr != NULL) {
				if (strPos <= 0)
					return ERROR_SYNTAX;
				char tmp = readRAM(e->avl_end + sizeof(AVL_Node));
				addr = dimStr(newstr, strPos);
				writeRAM(addr + sizeof(AVL_Node), tmp);
			} else {
				for (int i = 0; i < strPos; i++) {
					writeStr(addr, i, readRAM(i + e->avl_end + sizeof(AVL_Node)));
				}
				if (strPos != strSize) {
					writeStr(addr, strPos, 0);
				}
			}
		}
	} else {
		//Syntax error.
		return ERROR_SYNTAX;
	}

	return 0;
}

//Evaluate a conditional.
//	0	No error.
char evalEnd(Evaluator *e) {
	char c;
	ibword pos = 0;

	//Skip over white space.
	c = e->line_buff[pos];
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}

	//Syntax error.
	if (isEOL(c) || (c != 'E' && c != 'e'))
		return ERROR_SYNTAX;
	c = e->line_buff[++pos];
	if (isEOL(c) || (c != 'N' && c != 'n'))
		return ERROR_SYNTAX;
	c = e->line_buff[++pos];
	if (isEOL(c) || (c != 'D' && c != 'd'))
		return ERROR_SYNTAX;

	//Skip over white space.
	c = e->line_buff[++pos];
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}

	//Syntax error.
	if (!isEOL(c))
		return ERROR_SYNTAX;

	return 0;
}

//Evaluate a conditional.
//	0	No error.
char evalConditional(Evaluator *e) {
	ibword lhsStart = 0;
	ibword lhsSize = 0;
	ibword rhsStart = 0;
	ibword rhsSize = 0;
	char cond1 = ' ';
	char cond2 = ' ';
	char c;
	unsigned char pos = 0;
	char err;

	//Skip over white space.
	c = e->line_buff[pos];
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}

	//Syntax error.
	if (isEOL(c))
		return ERROR_SYNTAX;

	//Skip over command.
	while (!isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}

	//Syntax error.
	if (isEOL(c))
		return ERROR_SYNTAX;

	//Skip over white space.
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}

	//Get LHS.
	lhsStart = pos;
	while (!isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
		lhsSize++;
	}

	//Syntax error.
	if (isEOL(c))
		return ERROR_SYNTAX;
        //short str_size, str_pos;
        //bool str;

	//Skip over white space.
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}

	//Syntax error.
	if (isEOL(c))
		return ERROR_SYNTAX;

	//Get the first conditional.
	cond1 = c;
	c = e->line_buff[++pos];

	//Syntax error.
	if (isEOL(c))
		return ERROR_SYNTAX;

	//Get the second conditional.
	if (!isWS(c)) {
		cond2 = c;
		c = e->line_buff[++pos];
	}

	//Syntax error.
	if (isEOL(c) || !isWS(c))
		return ERROR_SYNTAX;

	//Skip over white space.
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}

	//Get RHS.
	rhsStart = pos;
	while (!isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
		rhsSize++;
	}

	//Skip over white space.
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}

	//Check for "THEN"
	if (isEOL(c) || (c != 't' && c != 'T'))
		return ERROR_MISSING_THEN;
	c = e->line_buff[++pos];
	if (isEOL(c) || (c != 'h' && c != 'H'))
		return ERROR_MISSING_THEN;
	c = e->line_buff[++pos];
	if (isEOL(c) || (c != 'e' && c != 'E'))
		return ERROR_MISSING_THEN;
	c = e->line_buff[++pos];
	if (isEOL(c) || (c != 'n' && c != 'N'))
		return ERROR_MISSING_THEN;
	c = e->line_buff[++pos];

	//Skip over whitespace.
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}

	//Syntax error.
	if (!isEOL(c))
		return ERROR_SYNTAX;

	//Check conditional.
	char lhsType = getExprType(e, lhsStart, lhsSize);
	char rhsType = getExprType(e, rhsStart, rhsSize);
	if (lhsType == TYPE_NUM && rhsType == TYPE_NUM) {
		float lhs, rhs;
		if (!verifyFormula(e, lhsStart, lhsSize))
			return ERROR_SYNTAX;
		err = copyFormulaIntoEvalBuff(e, lhsStart, lhsSize);
		if (err != 0) return err;
		lhs = evaluateFormula(e);
		if (!verifyFormula(e, rhsStart, rhsSize))
			return ERROR_SYNTAX;
		err = copyFormulaIntoEvalBuff(e, rhsStart, rhsSize);
		if (err != 0) return err;
		rhs = evaluateFormula(e);
		//Check the conditions.
		if (cond1 == '>' && cond2 == ' ') {
			if (!(lhs > rhs))
				return ERROR_CONDITIONAL_TRIGGERED;
		} else if (cond1 == '>' && cond2 == '=') {
			if (!(lhs >= rhs))
				return ERROR_CONDITIONAL_TRIGGERED;
		} else if (cond1 == '<' && cond2 == ' ') {
			if (!(lhs < rhs))
				return ERROR_CONDITIONAL_TRIGGERED;
		} else if (cond1 == '<' && cond2 == '=') {
			if (!(lhs <= rhs))
				return ERROR_CONDITIONAL_TRIGGERED;
		} else if (cond1 == '=' && cond2 == '=') {
			if (!(lhs == rhs))
				return ERROR_CONDITIONAL_TRIGGERED;
		} else if (cond1 == '!' && cond2 == '=') {
			if (!(lhs != rhs))
				return ERROR_CONDITIONAL_TRIGGERED;
		} else {
			return ERROR_SYNTAX;
		}
	} else if (lhsType == TYPE_STR || rhsType == TYPE_STR) {
		char compSize = 8;
		char compA[compSize + 1];
		char compB[compSize + 1];
		unsigned char compPosA = 0;
		unsigned char compPosB = 0;
		char c;

		//Load string A.
		if (!verifyString(e, lhsStart, lhsSize))
			return ERROR_SYNTAX;
		err = copyStringIntoEvalBuff(e, lhsStart, lhsSize);
		if (err != 0) return err;
		while ( (c = readCharFromEvalBuff(e)) ) {
			compA[compPosA++] = c;
			if (compPosA == compSize)
				break;
		}
		compA[compPosA] = 0;

		//Load string B.
		if (!verifyString(e, rhsStart, rhsSize))
			return ERROR_SYNTAX;
		err = copyStringIntoEvalBuff(e, rhsStart, rhsSize);
		if (err != 0) return err;
		while ( (c = readCharFromEvalBuff(e)) ) {
			compB[compPosB++] = c;
			if (compPosB == compSize)
				break;
		}
		compB[compPosB] = 0;

		//Check the conditions.
		if (cond1 == '=' && cond2 == '=') {
			if (strcmp(compA, compB) != 0)
				return ERROR_CONDITIONAL_TRIGGERED;
		} else if (cond1 == '!' && cond2 == '=') {
			if (strcmp(compA, compB) == 0)
				return ERROR_CONDITIONAL_TRIGGERED;
		} else {
			return ERROR_SYNTAX;
		}
	}

	return ERROR_CONDITIONAL_UNTRIGGERED;

}
//Evaluate a declaration.
//	0	No error.
char evalDeclaration(Evaluator *e) {
	char c;
	char key[KEY_SIZE];
	unsigned char keyPos = 0;
	ibword pos = 0;
	char ret = 0;
	bool containsAssignment = false;

	//Skip over white space.
	c = e->line_buff[pos];
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}
	//Syntax error.
	if (isEOL(c))
		return ERROR_SYNTAX;

	//Skip over "dim".
	pos += 3;

	//Syntax error.
	c = e->line_buff[pos];
	if (!isWS(c) || isEOL(c))
		return ERROR_SYNTAX;

	//Skip over white space.
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}

	//Syntax error.
	if (isEOL(c))
		return ERROR_SYNTAX;

	//Read in key.
	key[keyPos++] = c;
	c = e->line_buff[++pos];
	while (!isWS(c) && !isEOL(c) && c != '[' && c != '=') {
		key[keyPos++] = c;
		if (keyPos > KEY_SIZE)
			return ERROR_INVALID_KEY;
		c = e->line_buff[++pos];
	}
	key[keyPos] = 0;

	//Check to see if variable name is valid.
	if (!verifyKey(key))
		return ERROR_INVALID_KEY;

	//Make sure the variable isn't an address.
	if (key[0] == '@')
		return ERROR_INVALID_TYPE;

	//Check to see if variable already exists.
	if (findNode(key) != (ibword)undefined)
		return ERROR_KEY_EXISTS;

	//Syntax error.
	char numBuff[11];
	unsigned char numPos = 0;
	char hasArray = 0;
	char smartString = 0;
	ibword size = 0;
	ibword arrayStart = pos;
	ibword arrayEnd = pos;

	//Handle array.
	if (c == '[') {
		hasArray = 1;

		//Read in size.
		char numIsVar = 0;
		char isOnlyNum = 1;
		c = e->line_buff[++pos];
		while (c != ']' && !isEOL(c)) {
			if (isEOL(c))
				return ERROR_SYNTAX;
			if (isAlpha(c))
				numIsVar = 1;
			if (!isNum(c) && !isWS(c))
				isOnlyNum = 0;
			if (!isWS(c))
				numBuff[numPos++] = c;
			c = e->line_buff[++pos];
		}
		if (numPos == 0)
			return ERROR_SYNTAX;
		numBuff[numPos] = 0;
		if (numIsVar) {
			if (!verifyKey(numBuff))
				return ERROR_SYNTAX;
			ibword addrIndex = findNode(numBuff);
			if (addrIndex == (ibword)undefined)
				return ERROR_KEY_NOT_FOUND;
			size = (int)readNum(addrIndex);
		} else {
			if (isOnlyNum)
				size = atoi(numBuff);
			else
				return ERROR_SYNTAX;
		}
		arrayEnd = pos;

		//Syntax error.
		if (c != ']')
			return ERROR_SYNTAX;
		c = e->line_buff[++pos];

		//Skip over white space.
		while (isWS(c) && !isEOL(c)) {
			c = e->line_buff[++pos];
		}

		//Syntax error.
		if (!isEOL(c) && c != '=')
			return ERROR_SYNTAX;
		else if (c == '=')
			containsAssignment = true;

		//Make sure the string isn't too massive.
		if (size > STRING_SIZE_MAX || size < 0)
			return ERROR_STRING_TOO_LARGE;
		if (key[0] != '$' && size == 0)
			return ERROR_STRING_TOO_LARGE;

		//Create the variable.
		if (key[0] == '$')
			dimStr(key, size);
		else
			dimArr(key, size);

	} else {

		//Skip over white space.
		while (isWS(c) && !isEOL(c)) {
			c = e->line_buff[++pos];
		}

		//Syntax error.
		if (!isEOL(c) && c != '=')
			ERROR_SYNTAX;
		else if (c == '=')
			containsAssignment = true;

		//Create the variable.
		if (key[0] == '$') {
			smartString = 1;
			if (!containsAssignment)
				dimStr(key, 0);
		} else
			dimNum(key, 0);
	}

	if (containsAssignment) {
		//Remove brackets.
		for (ibword i = arrayStart; i < arrayEnd; i++)
			e->line_buff[i] = ' ';

		//Skip over white space;
		pos = 0;
		c = e->line_buff[pos++];
		while (isWS(c) && !isEOL(c)) {
			c = e->line_buff[pos++];
		}
		//Remove "DIM".
		e->line_buff[pos - 1] = ' ';
		e->line_buff[pos] = ' ';
		e->line_buff[pos + 1] = ' ';

		//Evaluate assignment.
		if (smartString)
			ret = evalAssignment(e, key);
		else
			ret = evalAssignment(e, NULL);	
	}

	return ret;
}

bool compareIgnoreCase(const char *a, const char *b) {
	char ca = a[0];
	char cb = b[0];
	unsigned char p = 0;
	while (ca != 0 && cb != 0) {
		char d = ('a' - 'A');
		if (
			ca + d != cb &&
			ca - d != cb &&
			ca != cb
			) {
			return false;
		}
		p++;
		ca = a[p];
		cb = b[p];
	}
	if (ca == 0 && cb == 0)
		return true;
	return false;
}

//Evaluates a command.
//Making arrays global saves program space.
#ifdef ARDUINO
#define COMMAND_SIZE_MAX 10
char outputKey[KEY_SIZE + 1];
ibword argsStart[10];
ibword argsSize[10];
char argsType[10];
char command[COMMAND_SIZE_MAX];
#endif
char evalCommand(Evaluator *e) {
	#ifdef DESKTOP
	#define COMMAND_SIZE_MAX 20
	char outputKey[KEY_SIZE + 1];
	ibword argsStart[10];
	ibword argsSize[10];
	char argsType[10];
	char command[COMMAND_SIZE_MAX];
	#endif
	ibword outputAddress = undefined;
	unsigned char arg = 1;
	char c;

	unsigned char commandPos = 0;
	ibword pos = 0;
	bool isInQuotes = false;

	//Get start of command.
	c = e->line_buff[pos];
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}
	argsStart[0] = pos;
	//Get end of command.
	while (!isWS(c) && !isEOL(c)) {
		if (!isAlpha(c) && c != '$')
			return ERROR_SYNTAX;
		if (commandPos == COMMAND_SIZE_MAX)
			return ERROR_UNKNOWN_COMMAND;
		command[commandPos++] = c;
		if (commandPos == COMMAND_SIZE_MAX)
			return ERROR_ARGUMENT_COUNT;
		c = e->line_buff[++pos];
	}
	command[commandPos] = 0;
	argsSize[0] = pos - argsStart[0];

	//Get arguments.
	while (!isEOL(c)) {
		//Get start of argument.
		while ( (isWS(c) || c == ',') && !isEOL(c)) {
			c = e->line_buff[++pos];
		}
		argsStart[arg] = pos;

		//Keep track if it only contains white space.
		char onlyWhiteSpace = 1;

		//Get end of argument.
		while ( (c != ',' || isInQuotes) && !isEOL(c) ) {
			if (c == '\"') {
				isInQuotes = !isInQuotes;
			}
			if (!isWS(c))
				onlyWhiteSpace = 0;
			c = e->line_buff[++pos];
		}
		argsSize[arg] = pos - argsStart[arg];

		//Update argument only if not only white space.
		if (!onlyWhiteSpace)
			arg++;
	}

	//Check for arrow.
	bool hasArrow = false;
	if (arg >= 2) {
		if (e->line_buff[argsStart[arg - 1]] == '=' &&
			e->line_buff[argsStart[arg - 1] + 1] == '>') {
			arg--;
		}

		c = e->line_buff[--pos];
		while (pos != argsSize[0]) {
			if (c == '\"') break;
			if (c == '>') {
				c = e->line_buff[--pos];
				if (c == '=') {
					hasArrow = true;
					break;
				}
			}
			c = e->line_buff[--pos];
		}
	}

	//Remove arrow from arguments.
	if (hasArrow) {
		//Remove arrow from arguments.
		argsSize[arg - 1] = pos - argsStart[arg - 1] - 1;

		//Skip over whitespace.
		pos += 2;
		c = e->line_buff[pos++];
		while (isWS(c) && !isEOL(c)) {
			c = e->line_buff[pos++];
		}

		//Syntax error.
		if (isEOL(c))
			return ERROR_SYNTAX;

		//Read in key.
		unsigned char keyPos = 0;
		while (!isWS(c) && !isEOL(c)) {
			outputKey[keyPos++] = c;
			c = e->line_buff[pos++];
		}
		outputKey[keyPos] = 0;

		//Skip over whitespace.
		while (isWS(c) && !isEOL(c)) {
			c = e->line_buff[pos++];
		}

		//Syntax error.
		if (!isEOL(c))
			return ERROR_SYNTAX;

		//Check if the key is valid.
		if (!verifyKey(outputKey))
			return ERROR_INVALID_KEY;

		//Check if variable exists.
		outputAddress = findNode(outputKey);
		if (outputAddress == (ibword)undefined)
			return ERROR_KEY_NOT_FOUND;

	}

	//Verify types.
	for (unsigned char i = 1; i < arg; i++) {
		argsType[i] = getExprType(e, argsStart[i], argsSize[i]);
		if (argsType[i] == TYPE_NUM) {
			if (!verifyFormula(e, argsStart[i], argsSize[i]))
				return ERROR_SYNTAX;
		} else {
			if (!verifyString(e, argsStart[i], argsSize[i]))
				return ERROR_SYNTAX;
		}
	}
	
	#include "commands_dynamic.c"
	
	return ERROR_UNKNOWN_COMMAND;

}

//Identifies the type of line.
char identifyLineType(Evaluator *e) {
	ibword pos = 0;
	bool checkedM = false;
	bool isDeclaration = true;
	bool isConditional = true;
	bool isEnd = true;
	bool isSub = true;
	char c = e->line_buff[pos];
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}
	if (isEOL(c)) return TYPE_EMPTY;
	if (c == '@') return TYPE_REFERENCE;
	if (c == ';') return TYPE_COMMENT;
	if ( (!isAlpha(c)) && c != '$' )
		return TYPE_COMMAND;
	if (c != 'D' && c != 'd')
		isDeclaration = false;
	if (c != 'I' && c != 'i')
		isConditional = false;
	if (c != 'E' && c != 'e')
		isEnd = false;
	if (c != 'S' && c != 's')
		isSub = false;
	c = e->line_buff[++pos];
	if (c != 'I' && c != 'i')
		isDeclaration = false;
	if (c != 'F' && c != 'f')
		isConditional = false;
	if (c != 'N' && c != 'n')
		isEnd = false;
	if (c != 'U' && c != 'u')
		isSub = false;
	char nc = e->line_buff[pos + 1];
	if (isConditional && (isWS(nc) || isEOL(nc)))
		return TYPE_CONDITIONAL;
	while (isAlphaNum(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
		
		if (!checkedM) {
			nc = e->line_buff[pos + 1];
			if ((c != 'M' && c != 'm') || !isWS(nc))
				isDeclaration = false;
			if ((c != 'D' && c != 'd'))
				isEnd = false;
			if ((c != 'B' && c != 'b'))
				isSub = false;
		}
		checkedM = true;
	}
	if (isDeclaration && c != '=')
		return TYPE_DECLARATION;
	if (isEnd && c != '=')
		return TYPE_END;
	if (isSub && c != '=')
		return TYPE_SUBROUTINE;
	if (isEOL(c)) return TYPE_COMMAND;
	while (isWS(c) && !isEOL(c)) {
		c = e->line_buff[++pos];
	}
	if (isEOL(c)) return TYPE_COMMAND;
	if (c == '=' || c == '[') {
		c = e->line_buff[++pos];
		if (c != '>') return TYPE_ASSIGNMENT;
	}
	return TYPE_COMMAND;
}

//Evaluates a single line at a position.
//	The addr should be the address of the next line.
char evalLine(Evaluator *e, ibword pos, ibword addr) {

	char type = identifyLineType(e);
	char ret = 0;
	if (type == TYPE_DECLARATION) {
		ret = evalDeclaration(e);
	} else if (type == TYPE_ASSIGNMENT) {
		ret = evalAssignment(e, NULL);
	} else if (type == TYPE_REFERENCE) {
		ret = evalReference(e, addr);
	} else if (type == TYPE_COMMAND) {
		ret = evalCommand(e);
	} else if (type == TYPE_CONDITIONAL) {
		ret = evalConditional(e);
	} else if (type == TYPE_END) {
		ret = evalEnd(e);
	} else if (type == TYPE_SUBROUTINE) {
		ret = evalSubroutine(e, addr);
	}
	return ret;
}

//Pribwords an error message associated
//	with an error code.
void printError(char e) {
	switch (e) {
		case 1:
			printString("Syntax error");
			break;
		case 2:
			printString("Not found");
			break;
		case 3:
			printString("Invalid name");
			break;
		case 4:
			printString("Variable exists");
			break;
		case ERROR_OUT_OF_BOUNDS:
			printString("Out of bounds");
			break;
		case 6:
			printString("Invalid type");
			break;
		case 7:
			printString("Invalid arguments");
			break;
		case 10:
			printString("Invalid comparison");
			break;
		case ERROR_MISSING_THEN:
			printString("Missing THEN");
			break;
		case ERROR_STRING_TOO_LARGE:
			printString("Invalid size");
			break;
		case ERROR_UNKNOWN_COMMAND:
			printString("Unknown command");
			break;
		case ERROR_FILE_NOT_FOUND:
			printString("File not found");
			break;
		case ERROR_STACK:
			printString("Stack error");
			break;
		default:
			printString("Invalid mode");
			break;
	}

}

//Evaluates a program at position.
#define subStackSize 100
#ifdef ARDUINO
ibword subStack[subStackSize];
char subStackPos = 0;
char skipEnd[subStackSize];
char skipEndPos = 0;
#endif
char evalPos(Evaluator *e, ibword pos) {
	ibword size;
	char eof = 10;
	char skipMode = 0;
	ibword lineNum = 0;
	#ifdef DESKTOP
	ibword subStack[subStackSize];
	unsigned char subStackPos = 0;
	char skipEnd[subStackSize];
	unsigned char skipEndPos = 0;
	#endif
	skipEnd[0] = 0;
	while (eof == 10) {
		lineNum++;
		size = copyFileIntoLineBuff(e, pos);
		if (size == (ibword)-1) return ERROR_SYNTAX;
		eof = e->in_ram ? readRAM(pos+size-1) : readFileOnDevice(pos+size-1);
		char err;
		char type = identifyLineType(e);
		if (skipMode > 0) {
			if (type == TYPE_END) {
				skipMode--;
			} else if (type == TYPE_CONDITIONAL || type == TYPE_SUBROUTINE) {
				skipMode++;
			}
			err = 0;
		} else if (skipMode != (char)-1) {
			err = evalLine(e, pos, pos + size);
			if (type == TYPE_END) {
				if (skipEnd[skipEndPos] > 0) {
					skipEnd[skipEndPos]--;
				} else {
					if (subStackPos == 0)
						err = ERROR_STACK;
					else {
						subStackPos--;
						skipEndPos--;
						pos = subStack[subStackPos];
						eof = 10;
						continue;
					}
				}
			} else if (type == TYPE_CONDITIONAL && err == ERROR_CONDITIONAL_UNTRIGGERED) {
				skipEnd[skipEndPos]++;
			}
		} else {
			err = 0;
		}
		//Not an actual error.
		if (err == ERROR_CONDITIONAL_UNTRIGGERED)
			err = 0;
		if (err == ERROR_HALTING)
			break;
		if (err != 0) {
			//This error means something occurred
			//	that requires changing the address.
			if (err == ERROR_CHANGE_ADDRESS) {
				pos = (ibword)evaluateFormula(e);
				eof = 10;
				skipEnd[skipEndPos] = 0;
				continue;
			} else if (err == ERROR_CHANGE_ADDRESS_CALL) {
				if (subStackPos == subStackSize) {
					err = ERROR_STACK;
					skipMode = -1;
				}
				subStack[subStackPos++] = pos + size;
				pos = (ibword)evaluateFormula(e);
				skipEnd[++skipEndPos] = 0;
				eof = 10;
				continue;
			} else if (err == ERROR_CONDITIONAL_TRIGGERED || err == ERROR_SUBROUTINE) {
				skipMode = 1;
			} else if (err == ERROR_SUBROUTINE) {
				skipMode = 1;
			} else {
				printError(err);
				printString(" at line ");
				printInt(lineNum);
				printString(".\n");
				return err;
			}
		}
		pos += size;
	}

	if (skipMode != 0 || skipEnd[skipEndPos] != 0) {
		printString("Missing END.\n");
		return 50;
	}

	return 0;
}

//Evaluates a single line.
char eval(Evaluator *e, char *line) {
	if (!copyStringIntoLineBuff(e, line))
		return ERROR_SYNTAX;
	return evalLine(e, 0, 0);
}
