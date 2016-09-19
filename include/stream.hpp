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

	template <typename Iter>
	_StreamItem(Iter begin, Iter end, size_t th = std::thread::hardware_concurrency()) : _pool(new Pool<void>(th)), _in(new O()), _out(_in) {
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
	typename _Mapper<typename O::ValueType, _M>::Ptr Map(const std::function<typename _SyncMap<_M>::PairType (typename O::ValueType)>& fn, size_t s = 1) {
		typename _Mapper<typename O::ValueType, _M>::Ptr item(new _Mapper<typename O::ValueType, _M>(_out, _pool));

		WaitGroup::Ptr wg(new WaitGroup(s));

		_pool->Send([item, fn, wg] {
			try {

				auto input = item->Input();
				auto output = item->Output();

				while (item->Input()->CanReceive()) {
					try {
						auto ret = fn(input->Pop(500));
						output->Insert(ret.first, ret.second);
					}
					catch (const ex::ClosedQueueException& ex) {
						std::cerr << ex.what() << std::endl;
					}
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

			while (input->CanReceive()) {
				auto ret = fn(input->Pop(500));
				output->Insert(ret.first, ret.second);
			}

			output->Close();
		});

		return item;
	}

	using Bouncer = _StreamItem<SyncQueue<typename O::ValueType>, SyncQueue<typename O::ValueType>>;

	typename Bouncer::Ptr Filter(const std::function<bool(typename O::ValueType)>& fn, size_t s = 1) {
		typename Bouncer::Ptr item(new Bouncer(_out, _pool));

		WaitGroup::Ptr wg(new WaitGroup(s));

		_pool->Send([item, fn, wg] {
			try {
				auto input = item->Input();
				auto output = item->Output();

				while (input->CanReceive()) {
					try {
						auto val = input->Pop(500);
						auto ret = fn(val);
						if (ret) {
							output->Push(val);
						}
					}
					catch (const ex::ClosedQueueException& ex) {
						std::cerr << ex.what() << std::endl;
					}
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

			while (input->CanReceive()) {
				auto val = input->Pop(500);
				auto ret = fn(val);
				if (ret) {
					output->Push(val);
				}
			}

			output->Close();
		});

		return item;
	}

	template <typename _O>
	using _Collector = _StreamItem<O, SyncQueue<_O>>;

	template <typename _O >
	typename _Collector<_O>::Ptr Collect(const std::function < _O(const typename O::Type&) > & fn, size_t s = 1) {
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

	template <typename _O>
	_O Reduce(const std::function<_O (const typename O::Type&, _O&)>& fn) {
		std::promise<_O> promise;
		auto result = promise.get_future();

		_pool->Send([this, &promise, &fn] {
			Output()->Wait();
			_O o = _O();
			Output()->ForEach([&o, &fn] (const typename O::Type& pair) {
				o = std::move(fn(pair, o));
			});

			promise.set_value(o);
		});

		return std::move(result.get());
	}

	void Close() {
		_out->Wait();
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
