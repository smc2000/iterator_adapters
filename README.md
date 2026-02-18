# Rust-like Iterator Adapters for C++


## Composables

Adapters can be combined to merge their effects together in sequence. These methods are known as "composables" and they only produce results upon iteration.

#### List of Composables:
| Composable | Description |
| --- | --- |
| Iterator | Plain adapter type. Has no additional effects. |
| Chain | Joins two compatible adapters together to a longer sequence. |
| Enumerate | Produces an incremental counter alongside the elements. |
| Filter | Produces only the elements that match a given condition. |
| Map | Converts each element to another value or type. |
| Reverse | Iterates elements in reverse order. |
| Skip | Iterate through all except the first N elements. |
| StepBy | Produces only a subset of elements. |
| Take | Iterate through only the first N elements. |
| Zip | Joins two adapters together to form a paired sequence. |

#### Examples:
```
std::vector<int> evens = {2,4,6,8};
std::vector<int> odds = {1,3,5,7,9};

// chain
auto chained = iter(&evens).chain(iter(&odds));
// 2,4,6,8,1,3,5,7,9

// enumerate
auto enumerated = iter(&evens).enumerate();
// [0,2],[1,4],[2,6],[3,8]

// filter
auto filtered = iter(&evens).filter([](int x){ return (3 < x) && (x < 7); });
// 4,6

// map
auto mapped = iter(&odds).map([](int x){ return x * x; });
// 1, 9, 25, 49, 81

// reverse
auto reversed = iter(&odds).reverse();
// 9,7,5,3,1

// skip
auto skipped = iter(&odds).skip(2);
// 5,7,9

// stepby
auto steppedby = iter(&odds).step_by(2);
// 1,5,9

// take
auto take = iter(&evens).take(3);
// 2,4,6

// zip
auto zipped = iter(&evens).zip(iter(&odds));
// [2,1],[4,3],[6,5],[8,7]
```

#### Combined Example:
```
std::vector<int> vec = {1,2,3,4,5};
std::vector<int> backwardsOddSquares = iter(&vec)
                                        .reverse()
                                        .filter([](int x){ return x & 1; })
                                        .map([](int x){ return x * x; })
                                        .collect<std::vector<int>>();
```
##### Above code, but without adapters:
```
std::vector<int> vec = {1,2,3,4,5};
std::vector<int> backwardsOddSquares;
for (auto iter = vec.rbegin(); iter != vec.rend(); ++iter) {
  int x = *iter;
  if (x & 1) {
    backwardsOddSquares.push_back(x * x);
  }
}
```

#### Usage in For Loops:
```
std::vector<int> vec = {1, 2, 3, 4, 5};
auto adapter = iter(&vec)
                 .enumerate()
                 .map([](auto pair) { return std::make_pair<int, float>(pair.first, pair.second / 10.0f); });
for (auto const& [k, v] : adapter) {
  printf("%d, %.2f\n", k, v);
}
```

## Terminating Methods
Aside from the composable methods, these methods exhaust the iterator and return a value.

#### List of Terminating Methods:
| Method | Description |
| --- | --- |
| all | Returns `true` if and only if all elements pass the test. |
| any | Returns `true` if one of the elements passes the test. |
| collect | Returns a container of type T holding all the elements. |
| count | Returns the number of elements iterated over. |
| find | Returns the first element that passes the test, if one exists. |
| fold | Recursively applies a function to each element and returns the result. |
| for_each | Applies a function to each element. |
| last | Returns the last element of the iterator, if one exists. |
| nth | Returns the element at index N, if one exists. |
| partition | Split the elements into two distinct containers of type T. |
| position | Returns the index of the first element that passes the test, if one exists. |

#### Examples:
```
std::vector<int> vec = {1,2,3,4,5,6,7,8,9};

bool all = iter(&vec).all([](int x){ return x > 0; });

bool any = iter(&vec).any([](int x){ return x == 5; });

auto uset = iter(&vec).collect<std::unordered_set<int>>();

size_t count = iter(&vec).count();

std::optional<int> find = iter(&vec).find([](int x){ return x == 9; });

int fold = iter(&vec).fold(0, [](int accum, int x){ return accum + x; });

iter(&vec).for_each([](int x){ printf("%d\n", x); })

std::optional<int> last = iter(&vec).last();

std::optional<int> nth = iter(&vec).nth(0);

auto partition = iter(&vec).partition<std::vector<int>>([](int x){ return x & 1; });

std::optional<int> position = iter(&vec).position([](int x){ return x == 0; });
```
