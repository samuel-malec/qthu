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
 * structure main
 * (
    * then_branch() = λ x -> outs
    * cons_10 -> b
    * mul x, b -> out
    * 
    * ----------------------------------
    * else_branch() = λ x -> out
    * cons_10 -> b
    * add x, b -> out
    * -----------------------------------
    * frame() = lambda A B x -> out
    * dup x -> x1 ,x2
    * fsig call A x1 -> out
    * fsig call B x2 -> out
    * 
    * ----------------------------------
    * run() = λ
    * cons_5 -> a
    * cons_10 -> b
    * 
    * le? a b -> cmp1
    * dup cmp1 -> cmp1 cmp2
    * not cmp2 -> cmp3
    * 
    * main then_branch -> then
    * main else_branch -> else
    * main frame ->       frame
    * 
    * opt cmp1 then_branch -> alt1
    * opt cmp2 then_branch -> alt2
    * 
    *---------------------------------------- 
    * )
 */