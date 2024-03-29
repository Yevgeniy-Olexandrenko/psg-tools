#pragma once

#include <cstdint>
#include <vector>

class Machine
{
public:
	using Addr = uint16_t;
	using Data = uint8_t;

	class Bus
	{
		static const Data OPEN = 0x00;

	public:
		class Handler
		{
		public:
			virtual void OnBusWrite(int tag, Addr addr, Data data);
			virtual Data OnBusRead(int tag, Addr addr) const;
		};

		void AddWriteReadHandler(Handler* handler, Addr from, Addr to, int tag);
		void AddWriteHandler(Handler* handler, Addr from, Addr to, int tag);
		void AddReadHandler(Handler* handler, Addr from, Addr to, int tag);
		
		void Write(Addr addr, Data data);
		Data Read(Addr addr);

	private:
		struct HandlerRange { Handler* handler; Addr from, to; int tag; };
		std::vector<HandlerRange> m_writeHandlers;
		std::vector<HandlerRange> m_readHandlers;
	};

	Bus& GetBus();

private:
	Bus m_bus;
};
