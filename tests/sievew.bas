? "Starting!"
NumIter = 1
sTime = TIME
DIM A(8190)
FOR Iter= 1 TO NumIter
  Count = 0
  FOR I = 0 TO 8190
    A(I) = 1
  NEXT I
  FOR I = 0 TO 8190
    IF A(I) = 1
      Prime = I + I + 3
      K = I + Prime
      WHILE K <= 8190
        A(K) = 0
        K = K + Prime
      WEND
      Count = Count + 1
    ENDIF
  NEXT I
NEXT Iter

eTime = TIME
? "End."
? "Elapsed time: "; eTime-sTime; " in "; NumIter; " iterations."
? "Found "; Count; " primes."
