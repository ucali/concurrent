#ifndef U_CONCURRENT_STREAM
#define U_CONCURRENT_STREAM

#include "pool.hpp"

namespace concurrent {
namespace /*_*/ {

template <typename I, typename O>
class _StreamItem {
public:
	typedef std::shared_ptr<_StreamItem<I, O>> Ptr;

	_StreamItem(size_t th = std::thread::hardware_concurrency()) : _pool(new Pool<void>(th)), _in(new O()), _out(_in) {}
	_StreamItem(Pool<void>::Ptr p) : _pool(p), _in(new O()), _out(_in) {}

	template <typename Iter>
	_StreamItem(Iter begin, Iter end, size_t th = std::thread::hardware_concurrency()) : _StreamItem(th) {
		_pool->Send([this, begin, end] {
			this->Stream(begin, end);
		});
	}

	template <typename Iter>
	_StreamItem(Iter begin, Iter end, Pool<void>::Ptr p) : _StreamItem(p) {
		_pool->Send([this, begin, end] {
			this->Stream(begin, end);
		});
	}

	_StreamItem(typename I::Ptr i, typename Pool<void>::Ptr p) : _in(i), _out(new O()), _pool(p) { }

	~_StreamItem() { }

	typename I::Ptr Input() { return _in; }
	typename O::Ptr Output() { return _out; }

	template <typename _I, typename _M>
	using _Mapper = _StreamItem<SyncQueue<_I>, _SyncMap<_M>>;

	template <typename _M>
	typename _Mapper<typename O::ValueType, _M>::Ptr KV(const std::function<typename _SyncMap<_M>::PairType(typename O::ValueType)>& fn) {
		typename _Mapper<typename O::ValueType, _M>::Ptr item(new _Mapper<typename O::ValueType, _M>(_out, _pool));

		_pool->Send([item, fn] {
			auto input = item->Input();
			auto output = item->Output();

			try {
				while (item->Input()->CanReceive()) {
					try {
						auto ret = fn(input->Pop(500));
						output->Insert(ret.first, ret.second);
					} catch (const ex::TimeoutQueueException& ex) {
					} catch (const ex::EmptyQueueException& ex) {
					}
				
				}
			} catch (const ex::ClosedQueueException& ex) {
				std::cerr << ex.what() << std::endl;
			}

			output->Close();
		});

		return item;
	}

	template <typename _M>
	typename _Mapper<typename O::ValueType, _M>::Ptr KV(const std::function<typename _SyncMap<_M>::PairType (typename O::ValueType)>& fn, size_t s) {
		typename _Mapper<typename O::ValueType, _M>::Ptr item(new _Mapper<typename O::ValueType, _M>(_out, _pool));

		WaitGroup::Ptr wg(new WaitGroup(s));

		_pool->Send([item, fn, wg] {
			try {

				auto input = item->Input();
				auto output = item->Output();

				try {
					while (item->Input()->CanReceive()) {
						try {
							auto ret = fn(input->Pop(500));
							output->Insert(ret.first, ret.second);
						} catch (const ex::TimeoutQueueException& ex) {
						} catch (const ex::EmptyQueueException& ex) {
						}

					}
				} catch (const ex::ClosedQueueException& ex) {
					std::cerr << ex.what() << std::endl;
				}
				wg->Finish();
			}
			catch (const std::exception& e) {
				wg->Finish();
				throw;
			}
		}, wg->Size());

		_pool->Send([fn, item, wg] {
			auto input = item->Input();
			auto output = item->Output();

			wg->Wait();

			try {
				while (input->CanReceive()) {
					try {
						auto ret = fn(input->Pop(500));
						output->Insert(ret.first, ret.second);
					} catch (const ex::TimeoutQueueException& ex) {
					} catch (const ex::EmptyQueueException& ex) {
					}
				}
			} catch (const ex::ClosedQueueException& ex) {
				std::cerr << ex.what() << std::endl;
			} 

			output->Close();
		});

		return item;
	}

	using Bouncer = _StreamItem<SyncQueue<typename O::ValueType>, SyncQueue<typename O::ValueType>>;

	typename Bouncer::Ptr Filter(const std::function<bool(typename O::ValueType)>& fn) {
		typename Bouncer::Ptr item(new Bouncer(_out, _pool));

		_pool->Send([item, fn] {
			auto input = item->Input();
			auto output = item->Output();

			while (input->CanReceive()) {
				try {
					try {
						auto val = input->Pop(500);
						auto ret = fn(val);
						if (ret) {
							output->Push(val);
						}
					} catch (const ex::TimeoutQueueException& ex) {
					} catch (const ex::EmptyQueueException& ex) {
					}

				}
				catch (const ex::ClosedQueueException& ex) {
					std::cerr << ex.what() << std::endl;
				}
			}
			output->Close();
		});

		return item;
	}

	typename Bouncer::Ptr Filter(const std::function<bool(typename O::ValueType)>& fn, size_t s) {
		typename Bouncer::Ptr item(new Bouncer(_out, _pool));

		WaitGroup::Ptr wg(new WaitGroup(s));

		_pool->Send([item, fn, wg] {
			try {
				auto input = item->Input();
				auto output = item->Output();

				try {
					while (input->CanReceive()) {
						try {
							auto val = input->Pop(500);
							auto ret = fn(val);
							if (ret) {
								output->Push(val);
							}
						} catch (const ex::TimeoutQueueException& ex) {
						} catch (const ex::EmptyQueueException& ex) {
						}
					}
				} catch (const ex::ClosedQueueException& ex) {
					std::cerr << ex.what() << std::endl;
				}
				wg->Finish();
			}
			catch (const std::exception& e) {
				wg->Finish();
				throw;
			}
		}, wg->Size());

		_pool->Send([fn, item, wg] {
			auto input = item->Input();
			auto output = item->Output();

			wg->Wait();

			try {
				while (input->CanReceive()) {
					try {
						auto val = input->Pop(500);
						auto ret = fn(val);
						if (ret) {
							output->Push(val);
						}
					} catch (const ex::TimeoutQueueException& ex) {
					} catch (const ex::EmptyQueueException& ex) {
					}
				}
			} catch (const ex::ClosedQueueException& ex) {
				std::cerr << ex.what() << std::endl;
			}

			output->Close();
		});

		return item;
	}

	template <typename _O>
	using _Collector = _StreamItem<O, SyncQueue<_O>>;

	template <typename _O >
	typename _Collector<_O>::Ptr Transform(const std::function < _O(const typename O::Type&) > & fn) {
		typename _Collector<_O>::Ptr item(new _Collector<_O>(_out, _pool));

		_pool->Send([item, fn] {
			auto input = item->Input();
			auto output = item->Output();

			input->Wait();

			input->ForEach([output, item, fn](const typename O::Type& v) {
				auto o = fn(v);
				output->Push(o);
			});

			item->Output()->Close();
			item->Input()->Clear();

		});
		return item; 
	}


	template <typename _O >
	typename _Collector<_O>::Ptr Transform(const std::function < _O(const typename O::Type&) > & fn, size_t s) {
		typename _Collector<_O>::Ptr item(new _Collector<_O>(_out, _pool));

		WaitGroup::Ptr wg(new WaitGroup(s));

		_pool->Send([item, fn, wg] {
			try {
				auto input = item->Input();
				auto output = item->Output();

				input->Wait();

				input->ForEach([output, item, fn] (const typename O::Type& v) {
					auto o = fn(v);
					output->Push(o);
				});

				wg->Finish();
			}
			catch (const std::exception& e) {
				wg->Finish();
				throw;
			}
		}, wg->Size());

		_pool->Send([item, wg] {
			wg->Wait();

			item->Input()->Clear();
			item->Output()->Close();
		});

		return item;
	}

	template <typename Out>
	using Partitioner = _StreamItem<O, SyncQueue<Out>>;

	template <typename Storage, typename Out>
	typename Partitioner<Out>::Ptr Partition(const std::function<Out (const typename O::KeyType&, std::shared_ptr<Storage>)>& fn) {
		typename Partitioner<Out>::Ptr item(new Partitioner<Out>(_out, _pool));

		_pool->Send([item, fn] {
			auto input = item->Input();
			input->Wait();

			auto output = item->Output();

			std::function<void(const typename O::KeyType&, std::shared_ptr<Storage>)> f = [output, fn](const auto& k, auto s) {
				output->Push(std::move(fn(k, s)));
			};

			input->Aggregate(f);
			output->Close();

		});
		return item;
	}

	template <typename Storage, typename Out>
	typename Partitioner<Out>::Ptr PartitionMT(const std::function<Out(const typename O::KeyType&, std::shared_ptr<Storage>)>& fn) {
		typename Partitioner<Out>::Ptr item(new Partitioner<Out>(_out, _pool));

		auto p = _pool;
		_pool->Send([p, item, fn] {
			auto input = item->Input();
			input->Wait();

			auto output = item->Output();

			std::function<void(const typename O::KeyType&, std::shared_ptr<Storage>)> f = [output, fn](const auto& k, auto s) {
				output->Push(fn(k, s));
			};

			std::function<void(const typename O::KeyType&, std::shared_ptr<Storage>)> main = [p, f](const auto& k, auto s) {
				auto fun = std::bind(f, k, s);
				p->Send(fun);
			};

			input->Aggregate(main);
			p->Send([output] {
				output->Close();
			});

		});
		return item;
	}


	template <typename _O>
	_O Reduce(const std::function<void (const typename O::Type&, _O&)>& fn) {
		std::promise<_O> promise;
		auto result = promise.get_future();

		_pool->Send([this, &promise, &fn] {
			Output()->Wait();
			_O o = _O();
			Output()->ForEach([&o, &fn] (const typename O::Type& pair) {
				fn(pair, o);
			});

			promise.set_value(o);
		});

		return std::move(result.get());
	}

	void Close() {
		_out->Wait();
		_pool->Close();
	}

	void Wait() {
		_out->WaitForEmpty();
		_pool->Close();
	}

	template <typename C>
	void Stream(const C& c) {
		for (const auto& item : c) {
			_in->Push(item);
		}
		_in->Close();
	}

	template <typename Iter>
	void Stream(Iter b, Iter e) {
		for (; b != e; b++) {
			_in->Push(*b);
		}
		_in->Close();
	}

	void ForEach(const std::function<void(const typename O::Type&)>& fn) {
		Output()->Wait();
		Output()->ForEach(fn);
	}

private:
	typename Pool<void>::Ptr _pool;

	typename I::Ptr _in;
	typename O::Ptr _out;

	_StreamItem(_StreamItem const&) = delete;
	_StreamItem& operator=(_StreamItem const&) = delete;
};

}

template <typename _I>
using Streamer = _StreamItem<SyncQueue<_I>, SyncQueue<_I>>;

template <typename _I>
using Bouncer = _StreamItem<SyncQueue<_I>, SyncQueue<_I>>;

template <typename _I, typename _O>
using Reducer = _StreamItem<SyncQueue<_I>, _O>;
}

#endif
