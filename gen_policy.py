letters = ['1', '2', '4', '9', 'a', 'c']
forbidden = ['4', 'c']

addComma = False
resString = "bool.any(["
for l in letters:
	for r in letters:
		if (not ((l in forbidden) and (r in forbidden))):
			if (addComma):
				resString += ', '
			else:
				addComma = True
			resString += 'message.value == 0x' + l + '0' + r
resString += '])'
print(resString)