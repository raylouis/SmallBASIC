'

SUB EXSUB
  EXIT SUB
  ? "SUB: EXIT SUB FAILED"
END

? "TEST REPEAT"
REPEAT
  EXIT LOOP
  ? "REPEAT: EXIT LOOP FAILED"
UNTIL 1

? "TEST WHILE"
WHILE 1
  EXIT LOOP
  ? "WHILE: EXIT LOOP FAILED"
WEND

? "TEST FOR"
FOR I=1 TO 20
  EXIT FOR
  ? "FOR: EXIT FOR FAILED"
NEXT

? "TEST SUB"
EXSUB
? "DONE"

