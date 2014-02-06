twistedcpp
==========
Porting twisted to C++.

Note: Everything is very very alpha, yet. Help and projects very appreciated. Check the examples to see what is currenlty possible.


``` cpp
#include <twisted/reactor.hpp>
#include <twisted/basic_protocol.hpp>
#include <twisted/default_factory.hpp>

struct echo_protocol : twisted::basic_protocol<echo_protocol> {
    template<typename Iter>
    void on_message(Iter begin, Iter end) {
        send_message(begin, end);
    }
};

int main() {
    twisted::reactor reac;
    reac.listen_tcp(50000, twisted::default_factory<echo_protocol>());
    reac.run();
}
```
