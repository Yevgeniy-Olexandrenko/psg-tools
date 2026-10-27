// LH5 (LHA -lh5-) compressor and decompressor.
// Based on ar002 by Haruhiko Okumura (public domain): static Huffman
// coding (huf.c, maketbl.c, maketree.c) and a sliding dictionary with
// percolating update (encode.c). The bit stream format is unchanged,
// the state lives in objects and corrupt input is reported, not fatal.

#include "lh5.h"
#include <limits.h>
#include <string.h>
#include <memory>
#include <new>

namespace
{
	using lh5::Result;

	const int DICBIT    = 13;  // 12 (-lh4-) or 13 (-lh5-)
	const int DICSIZ    = (1 << DICBIT);
	const int MAXMATCH  = 256; // not more than UCHAR_MAX + 1
	const int THRESHOLD = 3;
	const int NC        = (UCHAR_MAX + MAXMATCH + 2 - THRESHOLD); // alphabet of chars and lengths
	const int CBIT      = 9;   // floor(log2(NC)) + 1
	const int CODE_BIT  = 16;  // max codeword length
	const int NP        = (DICBIT + 1);
	const int NT        = (CODE_BIT + 3);
	const int PBIT      = 4;   // smallest integer such that (1 << PBIT) > NP
	const int TBIT      = 5;   // smallest integer such that (1 << TBIT) > NT
	const int NPT       = (NT > NP ? NT : NP);

	struct Error
	{
		Result result;
	};

	void Fail(Result result)
	{
		throw Error{ result };
	}

	////////////////////////////////////////////////////////////////////////////
	// Decoder
	////////////////////////////////////////////////////////////////////////////

	class Decoder
	{
	public:
		Decoder(const uint8_t* in, size_t size, std::vector<uint8_t>& out, size_t limit);
		void Run();

	private:
		void FillBuf(int n);
		unsigned GetBits(int n);
		void MakeTable(int nchar, const uint8_t* bitlen, int tablebits, uint16_t* table);
		void ReadPtLen(int nn, int nbit, int ispecial);
		void ReadCLen();
		unsigned DecodeC();
		unsigned DecodeP();

	private:
		const uint8_t* m_in;
		size_t   m_size;
		size_t   m_pos;
		uint64_t m_bitsTotal;
		uint64_t m_bitsUsed;
		uint16_t m_bitbuf;
		uint16_t m_subbitbuf;
		int      m_bitcount;
		unsigned m_blocksize;

		std::vector<uint8_t>& m_out;
		size_t m_limit;

		uint16_t m_left[2 * NC - 1];
		uint16_t m_right[2 * NC - 1];
		uint8_t  m_cLen[NC];
		uint8_t  m_ptLen[NPT];
		uint16_t m_cTable[4096];
		uint16_t m_ptTable[256];
	};

	Decoder::Decoder(const uint8_t* in, size_t size, std::vector<uint8_t>& out, size_t limit)
		: m_in(in)
		, m_size(size)
		, m_pos(0)
		, m_bitsTotal(uint64_t(size) * 8)
		, m_bitsUsed(0)
		, m_bitbuf(0)
		, m_subbitbuf(0)
		, m_bitcount(0)
		, m_blocksize(0)
		, m_out(out)
		, m_limit(limit)
	{
		memset(m_left, 0, sizeof(m_left));
		memset(m_right, 0, sizeof(m_right));
		memset(m_cLen, 0, sizeof(m_cLen));
		memset(m_ptLen, 0, sizeof(m_ptLen));
		memset(m_cTable, 0, sizeof(m_cTable));
		memset(m_ptTable, 0, sizeof(m_ptTable));
	}

	void Decoder::Run()
	{
		if (!m_size) return;

		FillBuf(16);
		m_bitsUsed = 0;

		while (!m_limit || m_out.size() < m_limit)
		{
			if (m_blocksize == 0)
			{
				// the stream ends at a block boundary, followed
				// only by the bits that pad it to a whole byte
				if (m_bitsTotal - m_bitsUsed < 16) break;

				m_blocksize = GetBits(16);
				if (m_blocksize == 0) m_blocksize = 0x10000;
				ReadPtLen(NT, TBIT, 3);
				ReadCLen();
				ReadPtLen(NP, PBIT, -1);
			}
			m_blocksize--;

			unsigned c = DecodeC();
			if (c <= UCHAR_MAX)
			{
				m_out.push_back(uint8_t(c));
			}
			else
			{
				size_t count = c - (UCHAR_MAX + 1 - THRESHOLD);
				size_t distance = DecodeP() + 1;
				if (distance > m_out.size()) Fail(Result::BadData);
				if (m_limit && count > m_limit - m_out.size()) count = m_limit - m_out.size();

				size_t from = m_out.size() - distance;
				while (count--) m_out.push_back(m_out[from++]);
			}
			if (m_bitsUsed > m_bitsTotal) Fail(Result::InputOverrun);
		}
	}

	void Decoder::FillBuf(int n)
	{
		// shift bitbuf n bits left, read n bits
		m_bitsUsed += n;
		m_bitbuf = uint16_t(m_bitbuf << n);
		while (n > m_bitcount)
		{
			n -= m_bitcount;
			m_bitbuf |= uint16_t(m_subbitbuf << n);
			m_subbitbuf = (m_pos < m_size ? m_in[m_pos++] : 0);
			m_bitcount = CHAR_BIT;
		}
		m_bitcount -= n;
		m_bitbuf |= uint16_t(m_subbitbuf >> m_bitcount);
	}

	unsigned Decoder::GetBits(int n)
	{
		if (n == 0) return 0;
		unsigned x = (m_bitbuf >> (16 - n));
		FillBuf(n);
		return x;
	}

	void Decoder::MakeTable(int nchar, const uint8_t* bitlen, int tablebits, uint16_t* table)
	{
		unsigned count[17], weight[17], start[18];

		for (int i = 1; i <= 16; i++) count[i] = 0;
		for (int i = 0; i < nchar; i++) count[bitlen[i]]++;

		start[1] = 0;
		for (int i = 1; i <= 16; i++)
			start[i + 1] = start[i] + (count[i] << (16 - i));
		if (start[17] != (1U << 16)) Fail(Result::BadData);

		int i, jutbits = 16 - tablebits;
		for (i = 1; i <= tablebits; i++)
		{
			start[i] >>= jutbits;
			weight[i] = 1U << (tablebits - i);
		}
		while (i <= 16)
		{
			weight[i] = 1U << (16 - i);
			i++;
		}

		unsigned size = 1U << tablebits;
		for (unsigned k = start[tablebits + 1] >> jutbits; k < size; k++) table[k] = 0;

		unsigned avail = nchar;
		unsigned mask = 1U << (15 - tablebits);
		for (int ch = 0; ch < nchar; ch++)
		{
			int len = bitlen[ch];
			if (len == 0) continue;

			unsigned nextcode = start[len] + weight[len];
			if (len <= tablebits)
			{
				if (nextcode > size) Fail(Result::BadData);
				for (unsigned k = start[len]; k < nextcode; k++) table[k] = uint16_t(ch);
			}
			else
			{
				unsigned k = start[len];
				uint16_t* p = &table[k >> jutbits];
				for (int n = len - tablebits; n != 0; n--)
				{
					if (*p == 0)
					{
						if (avail >= 2 * NC - 1) Fail(Result::BadData);
						m_right[avail] = m_left[avail] = 0;
						*p = uint16_t(avail++);
					}
					p = (k & mask) ? &m_right[*p] : &m_left[*p];
					k <<= 1;
				}
				*p = uint16_t(ch);
			}
			start[len] = nextcode;
		}
	}

	void Decoder::ReadPtLen(int nn, int nbit, int ispecial)
	{
		int n = GetBits(nbit);
		if (n == 0)
		{
			int c = GetBits(nbit);
			if (c >= nn) Fail(Result::BadData);
			for (int i = 0; i < nn; i++) m_ptLen[i] = 0;
			for (int i = 0; i < 256; i++) m_ptTable[i] = uint16_t(c);
		}
		else
		{
			if (n > nn) Fail(Result::BadData);

			int i = 0;
			while (i < n)
			{
				int c = (m_bitbuf >> 13);
				if (c == 7)
				{
					unsigned mask = 1U << 12;
					while (mask & m_bitbuf) { mask >>= 1; c++; }
					if (c > CODE_BIT) Fail(Result::BadData);
				}
				FillBuf(c < 7 ? 3 : c - 3);
				m_ptLen[i++] = uint8_t(c);

				if (i == ispecial)
				{
					c = GetBits(2);
					if (i + c > nn) Fail(Result::BadData);
					while (--c >= 0) m_ptLen[i++] = 0;
				}
			}
			while (i < nn) m_ptLen[i++] = 0;
			MakeTable(nn, m_ptLen, 8, m_ptTable);
		}
	}

	void Decoder::ReadCLen()
	{
		int n = GetBits(CBIT);
		if (n == 0)
		{
			int c = GetBits(CBIT);
			if (c >= NC) Fail(Result::BadData);
			for (int i = 0; i < NC; i++) m_cLen[i] = 0;
			for (int i = 0; i < 4096; i++) m_cTable[i] = uint16_t(c);
		}
		else
		{
			if (n > NC) Fail(Result::BadData);

			int i = 0;
			while (i < n)
			{
				int c = m_ptTable[m_bitbuf >> 8];
				if (c >= NT)
				{
					unsigned mask = 1U << 7;
					do
					{
						if (!mask) Fail(Result::BadData);
						c = (m_bitbuf & mask) ? m_right[c] : m_left[c];
						mask >>= 1;
					} while (c >= NT);
				}
				FillBuf(m_ptLen[c]);

				if (c <= 2)
				{
					if      (c == 0) c = 1;
					else if (c == 1) c = GetBits(4) + 3;
					else             c = GetBits(CBIT) + 20;

					if (i + c > NC) Fail(Result::BadData);
					while (--c >= 0) m_cLen[i++] = 0;
				}
				else m_cLen[i++] = uint8_t(c - 2);
			}
			while (i < NC) m_cLen[i++] = 0;
			MakeTable(NC, m_cLen, 12, m_cTable);
		}
	}

	unsigned Decoder::DecodeC()
	{
		unsigned j = m_cTable[m_bitbuf >> 4];
		if (j >= NC)
		{
			unsigned mask = 1U << 3;
			do
			{
				if (!mask) Fail(Result::BadData);
				j = (m_bitbuf & mask) ? m_right[j] : m_left[j];
				mask >>= 1;
			} while (j >= NC);
		}
		FillBuf(m_cLen[j]);
		return j;
	}

	unsigned Decoder::DecodeP()
	{
		unsigned j = m_ptTable[m_bitbuf >> 8];
		if (j >= NP)
		{
			unsigned mask = 1U << 7;
			do
			{
				if (!mask) Fail(Result::BadData);
				j = (m_bitbuf & mask) ? m_right[j] : m_left[j];
				mask >>= 1;
			} while (j >= NP);
		}
		FillBuf(m_ptLen[j]);
		if (j != 0) j = (1U << (j - 1)) + GetBits(j - 1);
		return j;
	}

	////////////////////////////////////////////////////////////////////////////
	// Encoder
	////////////////////////////////////////////////////////////////////////////

	class Encoder
	{
		using node = int16_t;

		static const int NIL = 0;
		static const int PERC_FLAG = 0x8000;
		static const int MAX_HASH_VAL = (3 * DICSIZ + (DICSIZ / 512 + 1) * UCHAR_MAX);
		static const int BUF_SIZE = 16 * 1024;

	public:
		Encoder(std::vector<uint8_t>& out, size_t limit);
		void Run(const uint8_t* in, size_t size);

	private:
		// sliding dictionary
		int  ReadText(uint8_t* p, int n);
		void InitSlide();
		node Child(node q, uint8_t c);
		void MakeChild(node q, uint8_t c, node r);
		void Split(node old);
		void InsertNode();
		void DeleteNode();
		void GetNextMatch();

		// static Huffman
		void HufEncodeStart();
		void HufEncodeEnd();
		void Output(unsigned c, unsigned p);
		void SendBlock();
		void CountTFreq();
		void WritePtLen(int n, int nbit, int ispecial);
		void WriteCLen();
		void EncodeC(int c);
		void EncodeP(unsigned p);

		// Huffman tree
		int  MakeTree(int nparm, uint16_t* freqparm, uint8_t* lenparm, uint16_t* codeparm);
		void CountLen(int i);
		void MakeLen(int root);
		void DownHeap(int i);
		void MakeCode(int n, const uint8_t* len, uint16_t* code);

		// output
		void PutBits(int n, unsigned x);
		void PutByte(uint8_t data);

	private:
		const uint8_t* m_in;
		size_t m_inSize;
		size_t m_inPos;

		std::vector<uint8_t>& m_out;
		size_t m_limit;
		unsigned m_subbitbuf;
		int m_bitcount;

		// sliding dictionary
		std::vector<uint8_t> m_text;
		std::vector<uint8_t> m_level;
		std::vector<uint8_t> m_childcount;
		std::vector<node> m_position;
		std::vector<node> m_parent;
		std::vector<node> m_prev;
		std::vector<node> m_next;
		node m_pos;
		node m_matchpos;
		node m_avail;
		int  m_remainder;
		int  m_matchlen;

		// static Huffman
		std::vector<uint8_t> m_buf;
		unsigned m_outputPos;
		unsigned m_outputMask;
		unsigned m_cpos;
		uint8_t  m_cLen[NC];
		uint8_t  m_ptLen[NPT];
		uint16_t m_cFreq[2 * NC - 1];
		uint16_t m_pFreq[2 * NP - 1];
		uint16_t m_tFreq[2 * NT - 1];
		uint16_t m_cCode[NC];
		uint16_t m_ptCode[NPT];

		// Huffman tree
		uint16_t  m_left[2 * NC - 1];
		uint16_t  m_right[2 * NC - 1];
		int16_t   m_heap[NC + 1];
		int       m_heapsize;
		int       m_n;
		int       m_depth;
		uint16_t* m_freq;
		uint16_t* m_sortptr;
		uint8_t*  m_len;
		uint16_t  m_lenCnt[17];
	};

	Encoder::Encoder(std::vector<uint8_t>& out, size_t limit)
		: m_in(nullptr)
		, m_inSize(0)
		, m_inPos(0)
		, m_out(out)
		, m_limit(limit)
		, m_subbitbuf(0)
		, m_bitcount(CHAR_BIT)
		, m_text(DICSIZ * 2 + MAXMATCH)
		, m_level(DICSIZ + UCHAR_MAX + 1)
		, m_childcount(DICSIZ + UCHAR_MAX + 1)
		, m_position(DICSIZ + UCHAR_MAX + 1)
		, m_parent(DICSIZ * 2)
		, m_prev(DICSIZ * 2)
		, m_next(MAX_HASH_VAL + 1)
		, m_pos(0)
		, m_matchpos(0)
		, m_avail(0)
		, m_remainder(0)
		, m_matchlen(0)
		, m_buf(BUF_SIZE)
		, m_outputPos(0)
		, m_outputMask(0)
		, m_cpos(0)
		, m_heapsize(0)
		, m_n(0)
		, m_depth(0)
		, m_freq(nullptr)
		, m_sortptr(nullptr)
		, m_len(nullptr)
	{
		memset(m_cLen, 0, sizeof(m_cLen));
		memset(m_ptLen, 0, sizeof(m_ptLen));
		memset(m_cFreq, 0, sizeof(m_cFreq));
		memset(m_pFreq, 0, sizeof(m_pFreq));
		memset(m_tFreq, 0, sizeof(m_tFreq));
		memset(m_cCode, 0, sizeof(m_cCode));
		memset(m_ptCode, 0, sizeof(m_ptCode));
		memset(m_left, 0, sizeof(m_left));
		memset(m_right, 0, sizeof(m_right));
		memset(m_heap, 0, sizeof(m_heap));
		memset(m_lenCnt, 0, sizeof(m_lenCnt));
	}

	void Encoder::Run(const uint8_t* in, size_t size)
	{
		m_in = in;
		m_inSize = size;
		m_inPos = 0;
		if (!size) return;

		InitSlide();
		HufEncodeStart();

		m_remainder = ReadText(&m_text[DICSIZ], DICSIZ + MAXMATCH);
		m_matchlen = 0;
		m_pos = DICSIZ;
		InsertNode();
		if (m_matchlen > m_remainder) m_matchlen = m_remainder;

		while (m_remainder > 0)
		{
			int  lastmatchlen = m_matchlen;
			node lastmatchpos = m_matchpos;

			GetNextMatch();
			if (m_matchlen > m_remainder) m_matchlen = m_remainder;

			if (m_matchlen > lastmatchlen || lastmatchlen < THRESHOLD)
			{
				Output(m_text[m_pos - 1], 0);
			}
			else
			{
				Output(lastmatchlen + (UCHAR_MAX + 1 - THRESHOLD),
					(m_pos - lastmatchpos - 2) & (DICSIZ - 1));
				while (--lastmatchlen > 0) GetNextMatch();
				if (m_matchlen > m_remainder) m_matchlen = m_remainder;
			}
		}
		HufEncodeEnd();
	}

	int Encoder::ReadText(uint8_t* p, int n)
	{
		size_t left = m_inSize - m_inPos;
		if (size_t(n) > left) n = int(left);
		memcpy(p, m_in + m_inPos, n);
		m_inPos += n;
		return n;
	}

	void Encoder::InitSlide()
	{
		for (int i = DICSIZ; i <= DICSIZ + UCHAR_MAX; i++)
		{
			m_level[i] = 1;
			m_position[i] = NIL; // sentinel
		}
		for (int i = DICSIZ; i < DICSIZ * 2; i++) m_parent[i] = NIL;
		m_avail = 1;
		for (int i = 1; i < DICSIZ - 1; i++) m_next[i] = node(i + 1);
		m_next[DICSIZ - 1] = NIL;
		for (int i = DICSIZ * 2; i <= MAX_HASH_VAL; i++) m_next[i] = NIL;
	}

	#define HASH(p, c) ((p) + ((c) << (DICBIT - 9)) + DICSIZ * 2)

	Encoder::node Encoder::Child(node q, uint8_t c)
	{
		// q's child for character c (NIL if not found)
		node r = m_next[HASH(q, c)];
		m_parent[NIL] = q; // sentinel
		while (m_parent[r] != q) r = m_next[r];
		return r;
	}

	void Encoder::MakeChild(node q, uint8_t c, node r)
	{
		// let r be q's child for character c
		node h = node(HASH(q, c));
		node t = m_next[h];
		m_next[h] = r; m_next[r] = t;
		m_prev[t] = r; m_prev[r] = h;
		m_parent[r] = q;
		m_childcount[q]++;
	}

	#undef HASH

	void Encoder::Split(node old)
	{
		node nw = m_avail;
		m_avail = m_next[nw];
		m_childcount[nw] = 0;

		node t = m_prev[old]; m_prev[nw] = t; m_next[t] = nw;
		t = m_next[old]; m_next[nw] = t; m_prev[t] = nw;

		m_parent[nw] = m_parent[old];
		m_level[nw] = uint8_t(m_matchlen);
		m_position[nw] = m_pos;
		MakeChild(nw, m_text[m_matchpos + m_matchlen], old);
		MakeChild(nw, m_text[m_pos + m_matchlen], m_pos);
	}

	void Encoder::InsertNode()
	{
		node q, r, t;

		if (m_matchlen >= 4)
		{
			m_matchlen--;
			r = node((m_matchpos + 1) | DICSIZ);
			while ((q = m_parent[r]) == NIL) r = m_next[r];
			while (m_level[q] >= m_matchlen)
			{
				r = q; q = m_parent[q];
			}

			t = q;
			while (m_position[t] < 0)
			{
				m_position[t] = m_pos; t = m_parent[t];
			}
			if (t < DICSIZ) m_position[t] = node(m_pos | PERC_FLAG);
		}
		else
		{
			q = node(m_text[m_pos] + DICSIZ);
			uint8_t c = m_text[m_pos + 1];
			if ((r = Child(q, c)) == NIL)
			{
				MakeChild(q, c, m_pos);
				m_matchlen = 1;
				return;
			}
			m_matchlen = 2;
		}

		for ( ; ; )
		{
			int j;
			if (r >= DICSIZ)
			{
				j = MAXMATCH;
				m_matchpos = r;
			}
			else
			{
				j = m_level[r];
				m_matchpos = node(m_position[r] & ~PERC_FLAG);
			}
			if (m_matchpos >= m_pos) m_matchpos -= DICSIZ;

			const uint8_t* t1 = &m_text[m_pos + m_matchlen];
			const uint8_t* t2 = &m_text[m_matchpos + m_matchlen];
			while (m_matchlen < j)
			{
				if (*t1 != *t2)
				{
					Split(r);
					return;
				}
				m_matchlen++; t1++; t2++;
			}
			if (m_matchlen >= MAXMATCH) break;

			m_position[r] = m_pos;
			q = r;
			if ((r = Child(q, *t1)) == NIL)
			{
				MakeChild(q, *t1, m_pos);
				return;
			}
			m_matchlen++;
		}

		t = m_prev[r]; m_prev[m_pos] = t; m_next[t] = m_pos;
		t = m_next[r]; m_next[m_pos] = t; m_prev[t] = m_pos;
		m_parent[m_pos] = q; m_parent[r] = NIL;
		m_next[r] = m_pos; // special use of next[]
	}

	void Encoder::DeleteNode()
	{
		node q, r, s, t, u;

		if (m_parent[m_pos] == NIL) return;
		r = m_prev[m_pos]; s = m_next[m_pos];
		m_next[r] = s; m_prev[s] = r;
		r = m_parent[m_pos]; m_parent[m_pos] = NIL;
		if (r >= DICSIZ || --m_childcount[r] > 1) return;

		t = node(m_position[r] & ~PERC_FLAG);
		if (t >= m_pos) t -= DICSIZ;

		s = t; q = m_parent[r];
		while ((u = m_position[q]) & PERC_FLAG)
		{
			u = node(u & ~PERC_FLAG);
			if (u >= m_pos) u -= DICSIZ;
			if (u > s) s = u;
			m_position[q] = node(s | DICSIZ);
			q = m_parent[q];
		}
		if (q < DICSIZ)
		{
			if (u >= m_pos) u -= DICSIZ;
			if (u > s) s = u;
			m_position[q] = node(s | DICSIZ | PERC_FLAG);
		}

		s = Child(r, m_text[t + m_level[r]]);
		t = m_prev[s]; u = m_next[s];
		m_next[t] = u; m_prev[u] = t;
		t = m_prev[r]; m_next[t] = s; m_prev[s] = t;
		t = m_next[r]; m_prev[t] = s; m_next[s] = t;
		m_parent[s] = m_parent[r]; m_parent[r] = NIL;
		m_next[r] = m_avail; m_avail = r;
	}

	void Encoder::GetNextMatch()
	{
		m_remainder--;
		if (++m_pos == DICSIZ * 2)
		{
			memmove(&m_text[0], &m_text[DICSIZ], DICSIZ + MAXMATCH);
			m_remainder += ReadText(&m_text[DICSIZ + MAXMATCH], DICSIZ);
			m_pos = DICSIZ;
		}
		DeleteNode();
		InsertNode();
	}

	void Encoder::HufEncodeStart()
	{
		m_buf[0] = 0;
		for (int i = 0; i < NC; i++) m_cFreq[i] = 0;
		for (int i = 0; i < NP; i++) m_pFreq[i] = 0;
		m_outputPos = m_outputMask = 0;
		m_bitcount = CHAR_BIT;
		m_subbitbuf = 0;
	}

	void Encoder::HufEncodeEnd()
	{
		SendBlock();
		PutBits(CHAR_BIT - 1, 0); // flush remaining bits
	}

	void Encoder::Output(unsigned c, unsigned p)
	{
		if ((m_outputMask >>= 1) == 0)
		{
			m_outputMask = 1U << (CHAR_BIT - 1);
			if (m_outputPos >= BUF_SIZE - 3 * CHAR_BIT)
			{
				SendBlock();
				m_outputPos = 0;
			}
			m_cpos = m_outputPos++;
			m_buf[m_cpos] = 0;
		}

		m_buf[m_outputPos++] = uint8_t(c);
		m_cFreq[c]++;

		if (c >= (1U << CHAR_BIT))
		{
			m_buf[m_cpos] |= m_outputMask;
			m_buf[m_outputPos++] = uint8_t(p >> CHAR_BIT);
			m_buf[m_outputPos++] = uint8_t(p);
			c = 0; while (p) { p >>= 1; c++; }
			m_pFreq[c]++;
		}
	}

	void Encoder::SendBlock()
	{
		unsigned root = MakeTree(NC, m_cFreq, m_cLen, m_cCode);
		unsigned size = m_cFreq[root];
		PutBits(16, size);

		if (root >= NC)
		{
			CountTFreq();
			root = MakeTree(NT, m_tFreq, m_ptLen, m_ptCode);
			if (root >= NT)
			{
				WritePtLen(NT, TBIT, 3);
			}
			else
			{
				PutBits(TBIT, 0);
				PutBits(TBIT, root);
			}
			WriteCLen();
		}
		else
		{
			PutBits(TBIT, 0);
			PutBits(TBIT, 0);
			PutBits(CBIT, 0);
			PutBits(CBIT, root);
		}

		root = MakeTree(NP, m_pFreq, m_ptLen, m_ptCode);
		if (root >= NP)
		{
			WritePtLen(NP, PBIT, -1);
		}
		else
		{
			PutBits(PBIT, 0);
			PutBits(PBIT, root);
		}

		unsigned pos = 0, flags = 0;
		for (unsigned i = 0; i < size; i++)
		{
			if (i % CHAR_BIT == 0) flags = m_buf[pos++]; else flags <<= 1;
			if (flags & (1U << (CHAR_BIT - 1)))
			{
				EncodeC(m_buf[pos++] + (1U << CHAR_BIT));
				unsigned k = m_buf[pos++] << CHAR_BIT;
				k += m_buf[pos++];
				EncodeP(k);
			}
			else EncodeC(m_buf[pos++]);
		}

		for (int i = 0; i < NC; i++) m_cFreq[i] = 0;
		for (int i = 0; i < NP; i++) m_pFreq[i] = 0;
	}

	void Encoder::CountTFreq()
	{
		for (int i = 0; i < NT; i++) m_tFreq[i] = 0;

		int n = NC;
		while (n > 0 && m_cLen[n - 1] == 0) n--;

		int i = 0;
		while (i < n)
		{
			int k = m_cLen[i++];
			if (k == 0)
			{
				int count = 1;
				while (i < n && m_cLen[i] == 0) { i++; count++; }
				if (count <= 2) m_tFreq[0] += uint16_t(count);
				else if (count <= 18) m_tFreq[1]++;
				else if (count == 19) { m_tFreq[0]++; m_tFreq[1]++; }
				else m_tFreq[2]++;
			}
			else m_tFreq[k + 2]++;
		}
	}

	void Encoder::WritePtLen(int n, int nbit, int ispecial)
	{
		while (n > 0 && m_ptLen[n - 1] == 0) n--;
		PutBits(nbit, n);

		int i = 0;
		while (i < n)
		{
			int k = m_ptLen[i++];
			if (k <= 6) PutBits(3, k);
			else PutBits(k - 3, (1U << (k - 3)) - 2);

			if (i == ispecial)
			{
				while (i < 6 && m_ptLen[i] == 0) i++;
				PutBits(2, (i - 3) & 3);
			}
		}
	}

	void Encoder::WriteCLen()
	{
		int n = NC;
		while (n > 0 && m_cLen[n - 1] == 0) n--;
		PutBits(CBIT, n);

		int i = 0;
		while (i < n)
		{
			int k = m_cLen[i++];
			if (k == 0)
			{
				int count = 1;
				while (i < n && m_cLen[i] == 0) { i++; count++; }
				if (count <= 2)
				{
					for (k = 0; k < count; k++)
						PutBits(m_ptLen[0], m_ptCode[0]);
				}
				else if (count <= 18)
				{
					PutBits(m_ptLen[1], m_ptCode[1]);
					PutBits(4, count - 3);
				}
				else if (count == 19)
				{
					PutBits(m_ptLen[0], m_ptCode[0]);
					PutBits(m_ptLen[1], m_ptCode[1]);
					PutBits(4, 15);
				}
				else
				{
					PutBits(m_ptLen[2], m_ptCode[2]);
					PutBits(CBIT, count - 20);
				}
			}
			else PutBits(m_ptLen[k + 2], m_ptCode[k + 2]);
		}
	}

	void Encoder::EncodeC(int c)
	{
		PutBits(m_cLen[c], m_cCode[c]);
	}

	void Encoder::EncodeP(unsigned p)
	{
		unsigned c = 0, q = p;
		while (q) { q >>= 1; c++; }
		PutBits(m_ptLen[c], m_ptCode[c]);
		if (c > 1) PutBits(c - 1, p & (0xFFFFU >> (17 - c)));
	}

	int Encoder::MakeTree(int nparm, uint16_t* freqparm, uint8_t* lenparm, uint16_t* codeparm)
	{
		// make tree, calculate len[], return root
		m_n = nparm; m_freq = freqparm; m_len = lenparm;
		int avail = m_n;
		m_heapsize = 0;
		m_heap[1] = 0;

		for (int i = 0; i < m_n; i++)
		{
			m_len[i] = 0;
			if (m_freq[i]) m_heap[++m_heapsize] = int16_t(i);
		}
		if (m_heapsize < 2)
		{
			codeparm[m_heap[1]] = 0;
			return m_heap[1];
		}

		// make priority queue
		for (int i = m_heapsize / 2; i >= 1; i--) DownHeap(i);

		int k;
		m_sortptr = codeparm;
		do
		{
			// while queue has at least two entries
			int i = m_heap[1]; // take out least-freq entry
			if (i < m_n) *m_sortptr++ = uint16_t(i);
			m_heap[1] = m_heap[m_heapsize--];
			DownHeap(1);

			int j = m_heap[1]; // next least-freq entry
			if (j < m_n) *m_sortptr++ = uint16_t(j);

			k = avail++; // generate new node
			m_freq[k] = uint16_t(m_freq[i] + m_freq[j]);
			m_heap[1] = int16_t(k);
			DownHeap(1); // put into queue
			m_left[k] = uint16_t(i);
			m_right[k] = uint16_t(j);
		} while (m_heapsize > 1);

		m_sortptr = codeparm;
		MakeLen(k);
		MakeCode(nparm, lenparm, codeparm);
		return k;
	}

	void Encoder::CountLen(int i)
	{
		if (i < m_n)
		{
			m_lenCnt[(m_depth < 16) ? m_depth : 16]++;
		}
		else
		{
			m_depth++;
			CountLen(m_left[i]);
			CountLen(m_right[i]);
			m_depth--;
		}
	}

	void Encoder::MakeLen(int root)
	{
		for (int i = 0; i <= 16; i++) m_lenCnt[i] = 0;
		m_depth = 0;
		CountLen(root);

		unsigned cum = 0;
		for (int i = 16; i > 0; i--)
			cum += m_lenCnt[i] << (16 - i);

		// limit code lengths to 16 bits
		while (cum != (1U << 16))
		{
			m_lenCnt[16]--;
			for (int i = 15; i > 0; i--)
			{
				if (m_lenCnt[i] != 0)
				{
					m_lenCnt[i]--;
					m_lenCnt[i + 1] += 2;
					break;
				}
			}
			cum--;
		}

		for (int i = 16; i > 0; i--)
		{
			int k = m_lenCnt[i];
			while (--k >= 0) m_len[*m_sortptr++] = uint8_t(i);
		}
	}

	void Encoder::DownHeap(int i)
	{
		// priority queue; send i-th entry down heap
		int j, k = m_heap[i];
		while ((j = 2 * i) <= m_heapsize)
		{
			if (j < m_heapsize && m_freq[m_heap[j]] > m_freq[m_heap[j + 1]]) j++;
			if (m_freq[k] <= m_freq[m_heap[j]]) break;
			m_heap[i] = m_heap[j];
			i = j;
		}
		m_heap[i] = int16_t(k);
	}

	void Encoder::MakeCode(int n, const uint8_t* len, uint16_t* code)
	{
		uint16_t start[18];
		start[0] = 0;
		start[1] = 0;
		for (int i = 1; i <= 16; i++)
			start[i + 1] = uint16_t((start[i] + m_lenCnt[i]) << 1);
		for (int i = 0; i < n; i++)
			code[i] = start[len[i]]++;
	}

	void Encoder::PutBits(int n, unsigned x)
	{
		// write rightmost n bits of x
		if (n < m_bitcount)
		{
			m_subbitbuf |= x << (m_bitcount -= n);
		}
		else
		{
			PutByte(uint8_t(m_subbitbuf | (x >> (n -= m_bitcount))));
			if (n < CHAR_BIT)
			{
				m_subbitbuf = x << (m_bitcount = CHAR_BIT - n);
			}
			else
			{
				PutByte(uint8_t(x >> (n - CHAR_BIT)));
				m_subbitbuf = x << (m_bitcount = 2 * CHAR_BIT - n);
			}
		}
	}

	void Encoder::PutByte(uint8_t data)
	{
		if (m_limit && m_out.size() >= m_limit) Fail(Result::OutputOverflow);
		m_out.push_back(data);
	}
}

namespace lh5
{
	const char* ToString(Result result)
	{
		switch (result)
		{
		case Result::Ok:             return "OK";
		case Result::BadData:        return "corrupt compressed data";
		case Result::InputOverrun:   return "unexpected end of compressed data";
		case Result::OutputOverflow: return "output size limit exceeded";
		case Result::BadHeader:      return "corrupt archive header";
		case Result::BadCrc:         return "CRC mismatch";
		case Result::Unsupported:    return "unsupported compression method";
		case Result::OutOfMemory:    return "out of memory";
		}
		return "unknown error";
	}

	Result Decompress(const uint8_t* in, size_t size, std::vector<uint8_t>& out)
	{
		size_t limit = out.capacity();
		out.clear();
		try
		{
			auto decoder = std::make_unique<Decoder>(in, size, out, limit);
			decoder->Run();
			return Result::Ok;
		}
		catch (const Error& error)
		{
			out.clear();
			return error.result;
		}
		catch (const std::bad_alloc&)
		{
			out.clear();
			return Result::OutOfMemory;
		}
	}

	Result Compress(const uint8_t* in, size_t size, std::vector<uint8_t>& out)
	{
		size_t limit = out.capacity();
		out.clear();
		try
		{
			auto encoder = std::make_unique<Encoder>(out, limit);
			encoder->Run(in, size);
			return Result::Ok;
		}
		catch (const Error& error)
		{
			out.clear();
			return error.result;
		}
		catch (const std::bad_alloc&)
		{
			out.clear();
			return Result::OutOfMemory;
		}
	}

	uint16_t Crc16(const uint8_t* data, size_t size, uint16_t crc)
	{
		static const struct Table
		{
			uint16_t values[256];
			Table()
			{
				for (unsigned i = 0; i < 256; i++)
				{
					unsigned r = i;
					for (int j = 0; j < CHAR_BIT; j++)
						r = (r & 1) ? (r >> 1) ^ 0xA001 : (r >> 1);
					values[i] = uint16_t(r);
				}
			}
		} table;

		while (size--)
			crc = uint16_t(table.values[(crc ^ *data++) & 0xFF] ^ (crc >> CHAR_BIT));
		return crc;
	}
}
