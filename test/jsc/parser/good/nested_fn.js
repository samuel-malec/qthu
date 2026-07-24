function outer()
{
    return twice( 10 );

    function twice( x )
    {
        return x + x;
    }
}