     addi r1,r1,15      ;set r1=15
     addi r10,r10,10   ;set r10=10, r10 is counter
loop and r1,r1,r10      ;r1 &= r10
     sw r1,r2,0      ;store r1, use r2 as offset
     addi r2,r2,1     ;offset ++
     addi r10,r10,-1   ;counter --
     beqz r10,end      ;loop
     j loop
end halt