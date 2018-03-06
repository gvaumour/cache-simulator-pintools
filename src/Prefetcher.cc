#include <vector>

#include "Prefetcher.hh"
#include "common.hh"
#include "Cache.hh"

using namespace std;

SimplePrefetcher::SimplePrefetcher(int prefetchDegree, int streams, bool stop_at_page ) : 
   Prefetcher(prefetchDegree, streams, stop_at_page)
{	
	m_prev_address.resize(streams);
}

void
SimplePrefetcher::generatePrefetch(uint64_t address, bool isHit, bool isInst)
{
/*	//DPRINTF("SimplePrefetcher::generatePrefetch");
	vector<uint64_t> prefetch_addresses = getNextAddress(address);
	
	for(auto p : prefetch_addresses)
	{
		Access request;
		request.m_address = p;
		request.m_type = isInst ? MemCmd::DATA_PREFETCH : MemCmd::INST_PREFETCH;
		prefetchAddress(request);
	}
*/
}


vector<uint64_t>
SimplePrefetcher::getNextAddress(uint64_t current_address)
{

	uint64_t distance = INT64_MAX;
	int index = PAGE_SIZE;
	vector<uint64_t> prefetch_addresses;
	
	for(unsigned i = 0; i < m_prev_address.size() ; i++)
	{
		if( (current_address - m_prev_address[i].addr < distance) && m_prev_address[i].isValid)
		{
			index = i;
			distance = current_address - m_prev_address[i].addr;
		}
	}
	//DPRINTF("SimplePrefetcher::getNextAddress Distance %ld, index %d\n", distance , index);
	
	if( !(distance > 0 && distance < PAGE_SIZE) ) //No address within proper distances, create an entry 
	{
		//Select the older entry
		uint64_t age = INT64_MAX;
		int index_old = 0;
		bool done = false;
		for(unsigned i = 0 ; i < m_prev_address.size();i++)
		{
			if(!m_prev_address[i].isValid)
			{
				m_prev_address[i].addr = current_address;
				m_prev_address[i].isValid = true;
				m_prev_address[i].LRU_info = cpt_time;
				done = true;
			}
		}
		if(!done)
		{
		
			for(unsigned i = 0 ; i < m_prev_address.size();i++)
			{
				if(m_prev_address[i].LRU_info < age)
				{
					age = m_prev_address[i].LRU_info;
					index_old = i;
				}
			}
			m_prev_address[index_old].addr = current_address;
			m_prev_address[index_old].isValid = true;
			m_prev_address[index_old].LRU_info = cpt_time;
		}
		//DPRINTF("SimplePrefetcher::getNextAddress No Address Generated\n");
		return prefetch_addresses;
	}
	
	
	int stride = current_address - m_prev_address[index].addr;
	m_prev_address[index].addr = current_address;
	m_prev_address[index].LRU_info = cpt_time;

	if (stride != 0)
	{
		//DPRINTF("SimplePrefetcher::getNextAddress Generated prefecth For address %#lx\n", current_address);

		for(int i = 0; i < m_prefetchDegree; ++i)
		{
			 uint64_t prefetch_address = current_address + (i+1) * stride;
			 // But stay within the page if requested
			 if (!m_stop_at_page || ((prefetch_address & PAGE_MASK) == (current_address & PAGE_MASK)))
			    prefetch_addresses.push_back(prefetch_address);
			//DPRINTF("SimplePrefetcher::getNextAddress - %#lx\n",prefetch_address );
		}
		
	}

	return prefetch_addresses;
}

void
SimplePrefetcher::printConfig(std::ostream &out)
{
	out << "Prefetcher:\tSimplePrefetcher" << endl;
	out << "Prefetch Degree\t" << m_prefetchDegree << endl;
	out << "NB Streams\t" << m_nbStreams << endl;
}

void
SimplePrefetcher::printStats(std::ostream &out)
{
//	out << "Prefetcher::Stats" << endl;
//	out << "Prefetcher::issuedPrefetch\t"<< m_stats_issuedPrefetchs << endl;
//	out << "Prefetcher::hitPrefecth\t"<< m_stats_hitsPrefetch << endl;
//	out << "Prefetcher::prefetchErrors\t"<< m_stats_issuedPrefetchs - m_stats_hitsPrefetch << endl;
}

