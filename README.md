# nmq
## design goals
``` c++
namespace nmq{

template<T, Iterator>
struct scheme{
	size_t capacity(const T&t);
    void pack(Iterator t, const T&t);
    T unpack(const Result&r);
};

template<T, Serialize=scheme<T, Message_ptr>>
struct tcp_transport{
  struct params {
    std::string host;
    unsigned short port;
  };

  using data_handler = std::function<void(const T &d, bool &cancel)>;
  using error_handler = std::function<void(const Message_ptr &d, const boost::system::error_code &err)>;

  struct chanel{
	void send_async(const T&message);
	void stop();
  };

  chanel listener(params p, data_handler onRecv, error_handler onErr);
  chanel connection(params p, data_handler onRecv, error_handler onErr);
};

template<T, Serialize=scheme<T, T>>
struct local_transport{
  struct params {
    
  };

  using data_handler = std::function<void(const T &d, bool &cancel)>;
  using error_handler = std::function<void(const T &d, const boost::system::error_code &err)>;

  struct chanel{
    void send_async(const T&message);
    void stop();
  };

  chanel listener(params p, data_handler onRecv, error_handler onErr);
  chanel connection(params p, data_handler onRecv, error_handler onErr);
};

template<T, Transport=tcp_transport<T>>
struct queue{
        struct consumer{
			using data_handler = std::function<void(const queue<T>&source, const T &d)>;;
            data_handler handler;
        };
        using transport_t=Transport;
		typename transport_t::chanel _chanel;

        queue(const std::string&name, typename transport_t::chanel _c){
			_chanel=_c;
		}

        void stop(){
			_chanel.stop;
		}

        void start(){
		}
        void publish(T);
        void add_consumer(const consumer&c);
};
}

namespace {
        struct Command{
            std::uint64_t id;
			std::string message;
			std::vector<std::uint16_t> values;
        };
		namespace nmq{
			template<>
			struct scheme<Command, Message_Ptr>{
				using Scheme = serialization::Scheme<uint64_t, std::string,std::vector<std::uint16_t>>;

                size_t capacity(const Command&c){
					return Scheme::capacity(c.id, c.message, c.values);
				}
				void pack(Message_Ptr t, const Command&c){
					Scheme::write(t->value(),  c.id, c.message, c.values);
				}
				Command unpack(const Message_Ptr&t){
					Command result;
					Scheme::read(t->value(), result.id, result.message, result.values);
					return result;
				}
			};

            template<>
            struct scheme<std::shared_ptr<Command>, std::shared_ptr<Command>>{
				size_t capacity(const Command&c){
					return sizeof(c);
				}
				void pack(Command t, const Command&c){
					return c;
				}
				Command unpack(const Command&t){
					return t;
				}
             };

		}
        using tcp_queue=queue<Command, tcp_transport<Command, Serialize=scheme<Command, Message_Ptr>>>;
		using local_queue=queue<std::shared_ptr<Command>, local_transport<std::shared_ptr<Command>>>;
		
		auto tcp_params=tcp_queue::transport_t::Params{"localhost", 4040};
		auto tcp_listener=tcp_queue::transport_t::listener(tcp_params, ...); 
		auto tcp_connection=tcp_queue::transport_t::connection(tcp_params, ...);

		auto listener_queue=tcp_queue("test1", tcp_listener);
        listener_queue.start();

        auto connection_queue=tcp_queue("test1", tcp_connection);
        connection_queue.start();


        auto local_params=local_queue::transport_t::Params{};
        auto local_listener=local_queue::transport_t::listener(local_params, ...);
        auto local_connection=local_queue::transport_t::connection(local_params, ...);

        auto local_listener_queue=local_queue("test1", local_listener);
        local_listener_queue.start();

        auto local_connection_queue=tcp_queue("test1", local_connection);
        local_connection_queue.start();
}
```

```C++
{
    using MyQueue=queue<MyMessage, MyResult, network::transport>;
    ...
    MyQueue::listener lstn;

    //translated to object FunctionHandler;
    lstn.consume([](const MyMessage&msg){
        return MyResult;
    });
}
```
