#ifndef PREFETCHER_H
#define PREFETCHER_H

#include <iostream>
#include <stdint.h>
#include <vector>

class Prefetcher 
{
	public: 
	
		Prefetcher(int prefetchDegree, int streams, bool stop_at_page) : m_prefetchDegree(prefetchDegree), \
		m_nbStreams(streams), m_stop_at_page(stop_at_page) {};
		virtual std::vector<uint64_t> getNextAddress(uint64_t current_address) = 0;
		virtual void generatePrefetch(uint64_t address, bool isHit, bool isInst) = 0;
		
		virtual void printConfig(std::ostream &out) = 0;
		virtual void printStats(std::ostream &out) = 0;

	protected:
		int m_prefetchDegree;
		int m_prefetchDepth;
		int m_nbStreams;
		bool m_stop_at_page;
};


class SimplePrefetcherEntry
{
	public:
	uint64_t addr;
	bool isValid;
	uint64_t LRU_info;
	
	SimplePrefetcherEntry(): addr(0), isValid(false), LRU_info(-1) {};
};


class SimplePrefetcher : public Prefetcher
{
   public:
	SimplePrefetcher(int prefetchDegree, int streams, bool stop_at_page);
	virtual std::vector<uint64_t> getNextAddress(uint64_t current_address);
	virtual void generatePrefetch(uint64_t address, bool isHit, bool isInst);
	
	virtual void printConfig(std::ostream &out);
	virtual void printStats(std::ostream &out);

   private:

	bool m_stop_at_page;
	std::vector<SimplePrefetcherEntry> m_prev_address;

//	uint64_t m_stats_issuedPrefetchs;
//	uint64_t m_stats_hitsPrefetch;
	
};
#endif
