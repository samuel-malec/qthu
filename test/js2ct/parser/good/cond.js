let x = 5;

if ( x < 10 )
{
    x = x * 10;
}

else
{
    x = x + 10;
}

let y = x;

/**
 * then_branch() = /\ x -> out
 * cons_10 -> b
 * mul x, b -> out
 * 
 * --------------------
 * else_branch() = /\ x -> out
 * cons_10 -> b
 * add x, b -> out
 * 
 * -------------------
 * run() = /\
 * cons_5 -> a
 * 
 * cons_10 -> b
 * 
 * le? a b -> cmp1
 * dup cmp1 -> cmp1 cmp2
 * not cmp2 -> cmp3
 * 
 * main then_branch -> then
 * main else_branch -> else
 * 
 * opt cmp1 then_branch -> alt1
 * opt cmp2 then_branch -> alt2
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 * 
 */