#pragma once


#include <streambuf>
#include <sstream>
#include <nngpp/msg_view.h>
#include <nngpp/view.h>


/*
	This header implements C++ iostreams for nng::msg.

	Currently unsupported features:
		- Seeking
		- Automatic append seeking

	Supported:
		- ios::trunc
		- ios

	streambuf -- msgbuf
	iostream  -- msgstream
	istream   -- imsgstream
	ostream   -- omsgstream

	basic_streambuf -- basic_msgbuf
	basic_iostream  -- basic_msgstream
	basic_istream   -- basic_imsgstream
	basic_ostream   -- basic_omsgstream
*/


namespace nng
{
	template<class Elem, class Traits = std::char_traits<Elem>>
	class basic_msgbuf : public std::basic_streambuf<Elem, Traits>
	{
	public:
		using openmode = std::ios::openmode;
		using _super = std::basic_streambuf<Elem, Traits>;

		using char_type   = typename _super::char_type;
		using int_type    = typename _super::int_type;
		using traits_type = typename _super::traits_type;
		using pos_type    = typename _super::pos_type;
		using off_type    = typename _super::off_type;


	public:
		basic_msgbuf()  noexcept    : _body(nullptr), _ncap(0) {}
		~basic_msgbuf() noexcept    {close();}

		basic_msgbuf* open(nng::msg_view msg, std::ios::openmode mode)    {return open(msg.body(), mode);}
		bool          is_open() const noexcept                            {return _body.get_msg();}
		void          close() noexcept                                    {if (is_open()) *this = basic_msgbuf();}

		/*
			Open a message for reading/writing.
		*/
		basic_msgbuf* open(nng::msg_body body, std::ios::openmode mode)
		{
			// Validate
			if (!body.get_msg() || !(mode & (std::ios::in|std::ios::out)))
				return nullptr;

			// Truncate
			if (mode & std::ios::trunc) _body.chop(_body.size());

			// Setup
			_body   = body;
			_ncap = (body.size()/sizeof(char_type));
			_mode   = mode;

			char_type *start = _start(),
				*gpos = start,
				*ppos = (((mode & std::ios::ate) | (mode & std::ios::app)) ? _g_end() : start);

			if (mode & std::ios::in)  this->setg(start, gpos, _g_end());
			if (mode & std::ios::out) this->setp(start, ppos, _p_end());

			return this;
		}

	protected:
		int sync() override
		{
			if (is_open())
			{
				_sync_length();
				return 0;
			}
			return -1;
		}

		std::streamsize xsputn(const char_type* s, std::streamsize n) override
		{
			if (!_is_writing() || !n) return 0;
			_sync_length();

			// Automatically grow message body
			size_t need_capac = _count()+n;
			if (need_capac > _capac())
			{
				if (need_capac < 32) need_capac = 32;
				if (need_capac < 2*_capac()) need_capac = 2*_capac();
				_grow(need_capac);
			}

			// Write data to message
			_body.append(nng::view(s, n));
			this->setp(_start(), this->pptr() + n, _p_end());
			return n;
		}

		std::streamsize xsgetn(char_type* s, std::streamsize n) override
		{
			if (!_is_reading()) return 0;
			_sync_length();

			// Stop at end of data
			char_type *end = _g_end(), *pos = this->gptr();
			if (pos + n < end) n = end-pos;

			// Read data from message
			std::memcpy((void*) s, (void*) pos, sizeof(char_type)*n);
			this->setg(_start(), pos + n, _g_end());
			return n;
		}

		std::streamsize showmanyc() override
		{
			if (_is_reading())
			{
				_sync_length();
				char_type *end = _g_end(), *pos = this->gptr();
				if (pos < end) return end-pos;
			}
			return -1;
		}

		int_type underflow() override
		{
			if (_is_reading())
			{
				_sync_length();

				// See if data is available.
				char_type *gpos = this->gptr(), *gend = _g_end();
				if (gpos < gend)
				{
					this->setg(_start(), gpos, gend);
					return *gpos;
				}
			}
			
			return traits_type::eof();
		}

		int_type overflow(int_type c) override
		{
			if (!_is_writing()) return traits_type::eof();

			// Decide on new capacity
			size_t new_capac = (_body.size()/sizeof(char_type)) * 2;
			if (new_capac < 32) new_capac = 32;

			_grow(new_capac);

			// Put character without advancing psoition.
			*this->pptr() = c;
			this->setp(_start(), this->pptr()+1, _p_end());
			return traits_type::to_int_type(c);
		}
		
		pos_type seekpos(std::streampos _pos, std::ios::openmode which = std::ios::in | std::ios::out) override
		{
			_sync_length();
			size_t pos = ((_pos < 0) ? 0 : size_t(_pos));
			if (pos > _count()) pos = _count();

			if (which & std::ios::in ) this->setg(_start(), _start()+pos, _g_end());
			if (which & std::ios::out) this->setp(_start(), _start()+pos, _g_end());

			return pos;
		}

		pos_type seekoff(off_type off, std::ios::seekdir way, std::ios::openmode which = std::ios::in | std::ios::out) override
		{
			char_type *gp, *pp;
			switch (way)
			{
			default:
			case std::ios::beg: gp = pp = _start(); break;
			case std::ios::end: _sync_length(); gp = pp = _g_end(); break;
			case std::ios::cur: gp = this->gptr(); pp = this->pptr(); break;
			}
			gp += off;
			pp += off;
			if (which & std::ios::in ) this->setg(_start(), gp, _g_end());
			if (which & std::ios::out) this->setp(_start(), pp, _p_end());

			if (which & std::ios::out) return pp-_start();
			else                       return gp-_start();
		}


	protected:
		nng::msg_body      _body;
		std::ios::openmode _mode = 0;
		size_t             _ncap = 0; // Known capacity (elements)

		char_type *_start() const    {return _body.data<char_type>();}
		char_type *_g_end() const    {return _body.data<char_type>() + _body.size()/sizeof(char_type);}
		char_type *_p_end() const    {return _start() + _ncap;}
		size_t     _capac() const    {return _p_end() - _start();}
		size_t     _count() const    {return _g_end() - _start();}

		bool _is_writing() const noexcept    {return _mode & std::ios::out;}
		bool _is_reading() const noexcept    {return _mode & std::ios::in;}

		void _sync_length() noexcept
		{
			if (this->pptr() > _g_end())
				nng_msg_realloc(_body.get_msg(), static_cast<char*>(this->pptr())-_body.data<char>());
		}

		void _grow(size_t new_capac)
		{
			if (new_capac < _capac()) return;

			// Reallocate message and update pointers
			size_t g_ind, p_ind, written_elems = _body.size() / sizeof(char_type);
			if (_is_reading()) g_ind = this->gptr() - this->eback();
			if (_is_writing()) p_ind = this->pptr() - this->pbase();

			// Reallocate message
			auto code = nng_msg_realloc(_body.get_msg(), new_capac * sizeof(char_type));
			_body.chop((new_capac - written_elems) * sizeof(char_type));
			if (code != 0) throw nng::exception(code, "nng_msg_realloc");

			// Update addresses
			char_type *start = _start();
			_ncap = new_capac;
			if (_is_reading()) this->setg(start, start+g_ind, _g_end());
			if (_is_writing()) this->setp(start, start+p_ind, _p_end());
		}
	};

	/*
		Implementation of istream/ostream/iostream
	*/
	template<class Elem, class Traits,
		class StreamBase,
		std::ios::openmode ModeDefault,
		std::ios::openmode ModeForce = 0>
	class basic_msgstream_ : protected basic_msgbuf<Elem, Traits>, public StreamBase
	{
	protected:
		using _mebuf = basic_msgbuf<Elem, Traits>;
		_mebuf *_asbuf() noexcept    {return static_cast<_mebuf*>(this);}


	public:
		void open(nng::msg_body msg, std::ios::openmode mode = ModeDefault)           {_mebuf::open(msg, mode&ModeForce);}
		void open(nng::msg_view msg, std::ios::openmode mode = ModeDefault)           {_mebuf::open(msg, mode&ModeForce);}
		void close() noexcept                                                         {_mebuf::close();}

		basic_msgstream_(const _mebuf &buf)                                           : StreamBase(_asbuf()), _mebuf(buf) {}
		basic_msgstream_(nng::msg_body msg, std::ios::openmode mode = ModeDefault)    : StreamBase(_asbuf()) {_mebuf::open(msg, mode&ModeForce);}
		basic_msgstream_(nng::msg_view msg, std::ios::openmode mode = ModeDefault)    : StreamBase(_asbuf()) {_mebuf::open(msg, mode&ModeForce);}

		basic_msgstream_()                                                            : StreamBase(_asbuf()) {}
		~basic_msgstream_() noexcept {}
		

		// Copyable, movable, etc
		basic_msgstream_(const basic_msgstream_ &o)                                   : _mebuf(o),            StreamBase(_asbuf()) {}
		basic_msgstream_(basic_msgstream_      &&o)                                   : _mebuf(std::move(o)), StreamBase(_asbuf()) {}
		basic_msgstream_ &operator=(const basic_msgstream_ &o)                        {*_asbuf() = o;            return *this;}
		basic_msgstream_ &operator=(basic_msgstream_      &&o)                        {*_asbuf() = std::move(o); return *this;}
	};


	template<class Elem, class Traits = std::char_traits<Elem>>
	using basic_msgstream = basic_msgstream_<Elem, Traits, std::basic_iostream<Elem, Traits>, std::ios::in|std::ios::out, 0>;
	template<class Elem, class Traits = std::char_traits<Elem>>
	using basic_imsgstream = basic_msgstream_<Elem, Traits, std::basic_istream<Elem, Traits>, std::ios::in,  std::ios::in>;
	template<class Elem, class Traits = std::char_traits<Elem>>
	using basic_omsgstream = basic_msgstream_<Elem, Traits, std::basic_ostream<Elem, Traits>, std::ios::out, std::ios::out>;


	/*
		msgbuf writing one byte at a time.
	*/
	using msgbuf     = basic_msgbuf<char>;
	using msgstream  = basic_msgstream<char>;
	using imsgstream = basic_imsgstream<char>;
	using omsgstream = basic_omsgstream<char>;


	/*
		Special output operators for nng::view.
	*/
	inline omsgstream &operator<<(omsgstream &out, const nng::view &v)    {out.write(v.data<char>(), v.size()); return out;}
	inline msgstream  &operator<<( msgstream &out, const nng::view &v)    {out.write(v.data<char>(), v.size()); return out;}

	template<size_t N>
	omsgstream &operator<<(omsgstream &out, const char (&s)[N])    {out.write(s, N-(s[N-1]==0)); return out;}
	template<size_t N>
	msgstream  &operator<<( msgstream &out, const char (&s)[N])    {out.write(s, N-(s[N-1]==0)); return out;}
}