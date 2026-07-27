function abs(x) {
    if (x < 0) {
        return 0 - x;
    } 
    else {
        return x;
    }
}

/*
structure abs
(
    then_0 = λ x → out
    (
        jsvalue cons_0     → zero
        jsvalue sub zero x → out
    )

    else_0 = λ x → out
    (
        jsvalue move x → out
    )

    frame_0 = λ A B x → out
    (
        jsvalue dup x    → x₁ x₂
        fᵢⁱ call A x₁ → out₁
        fᵢⁱ call B x₂ → out₂
        jsvalue join out₁ out₂ → out
    )

    run = λ x → out
    (
        jsvalue cons_0    → zero
        jsvalue dup x     → x₁ x₂
        jsvalue lt? x₁ zero → cmp
        jsvalue dup cmp   → cmp₁ cmp₂
        jsvalue not cmp₂  → cmp₃

        abs then_0  → then_ref
        abs else_0  → else_ref
        abs frame_0 → frame

        fᵢⁱ opt cmp₁ then_ref → alt₁
        fᵢⁱ opt cmp₃ else_ref → alt₂
        fᵢⁱ join alt₁ alt₂ frame → cont

        fᵢⁱ call cont x₂ → out
    )
)
*/
