     addi r2,r2,100      ;set r2=100
     addi r8,r8,10   ;set r8=10, r8 is counter
loop sub r2,r2,r8      ;r2 -= r8
     addi r8,r8,-1   ;counter --
     beqz r8,end      ;loop
     j loop
end halt