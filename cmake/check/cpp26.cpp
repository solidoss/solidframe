// C++26: pack indexing (P2662)
template <typename... Ts>
auto first(Ts... vs)
{
    return vs...[0];
}

int main()
{
    return first(0, 1, 2);
}
