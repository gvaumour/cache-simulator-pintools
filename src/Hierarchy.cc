#include <iostream>
#include <string>
#include <vector>

#include "Hierarchy.hh"
#include "HybridCache.hh"
#include "common.hh"

using namespace std;


Level::Level(int level, std::vector<ConfigCache> configs, Hierarchy* system) : m_level(level), m_system(system){
	assert(configs.size()  >  0);

	for(unsigned int i = 0 ; i < configs.size() ;  i++)
	{
		m_caches.push_back(HybridCache(configs[i].m_size, configs[i].m_assoc , \
						configs[i].m_blocksize, configs[i].m_nbNVMways, configs[i].m_policy, this));		
	} 
}

Level::Level(const Level& a)
{

	m_level = a.getLevel();
	m_system = a.getSystem();
	
	vector<HybridCache> dummy = a.getCaches();
	
	for(unsigned i = 0 ; i < dummy.size() ; i++)
	{
		m_caches.push_back( HybridCache(dummy[i].getSize(), dummy[i].getAssoc(), \
				dummy[i].getBlockSize(), dummy[i].getNVMways(), dummy[i].getPolicy(), this));	
	}
}

void
Level::handleAccess(Access element)
{
	if(element.isInstFetch() && m_caches.size() == 2)
		return m_caches[1].handleAccess(element);	
	else
		return m_caches[0].handleAccess(element);		
}

bool
Level::lookup(Access element)
{
	if(element.isInstFetch() && m_caches.size() == 2)
		return m_caches[1].lookup(element);
	else 
		return m_caches[0].lookup(element);	
}

void
Level::deallocate(uint64_t addr)
{
	for(auto p : m_caches)
		p.deallocate(addr);
}

void
Level::signalDeallocate(uint64_t addr)
{
	if(m_level > 0)
		m_system->deallocateFromLevel(addr , m_level);
}

void
Level::print(std::ostream& out) const
{
	out << "************************************************" << endl;
	out << "Level n°" << m_level << endl;
	for( unsigned i = 0 ; i < m_caches.size(); i++)
	{
		out << m_caches[i];
		out << "************************************************" << endl;
	}	
}

/*
void
Level::printResults()
{
	cout << "************************************************" << endl;
	cout << "Level n°" << m_level << endl;
	for( unsigned i = 0 ; i < m_caches.size(); i++)
	{
		//m_caches[i].printStats();
		cout << "************************************************" << endl;
	}
}*/



Hierarchy::Hierarchy(int nbLevel)
{
	ConfigCache L1Dataconfig (32768, 2 , 64 , "LRU", 0);
	vector<ConfigCache> firstLevel;
	firstLevel.push_back(L1Dataconfig);

	ConfigCache L1Instconfig = L1Dataconfig;
	firstLevel.push_back(L1Instconfig);

	ConfigCache L2config (262144, 8 , 64 , "preemptive", 4);
	vector<ConfigCache> secondLevel;
	secondLevel.push_back(L2config);
	
	//assert(nbLevel > 0);
	
	//m_nbLevel = nbLevel;
	
	m_nbLevel = 2;
	//m_levels.resize(m_nbLevel);
	m_levels.push_back( Level(0 , firstLevel, this) );
	m_levels.push_back( Level(1 , secondLevel, this) );
}


void
Hierarchy::print(std::ostream& out) const
{
	out << "ConfigCache : " << m_configFile << endl;
	out << "NbLevel : " << m_nbLevel << endl;
	for(unsigned i = 0 ; i < m_nbLevel ; i++)
	{
		out << "***************" << endl;
		m_levels[i].print(out);
	}
	out << "***************" << endl;
}

/*
void
Hierarchy::printResults()
{
	cout << "Hierarchy print result " << endl;
	for(unsigned int i = 0 ; i < m_nbLevel ; i++)
	{
		m_levels[i].printResults();
	}
}*/



void
Hierarchy::handleAccess(Access element)
{
	unsigned i= 0;
	bool hasData = false;
	
	//While the data is not found in the current level, transmit the request to the next level

	do
	{
		hasData = m_levels[i].lookup(element);
		i++;
	}while(!hasData && i < m_nbLevel);
	
	i--;
	
	DPRINTF("Hierarchy:: Data found in level %d\n",i);
		
	for(int a = i ; a >= 0 ; a--)
	{
		DPRINTF("\tHierarchy:: Handling data in level %d\n",a);
		m_levels[a].handleAccess(element);
	}
}


void
Hierarchy::deallocateFromLevel(uint64_t addr , int level)
{
	DPRINTF("Hierarchy::deallocateFromLevel %ld, level : %d\n" , addr, level);
	
	int i = level-1;
	while(i !=0)
	{
		i--;
		m_levels[i].deallocate(addr);
	}
}


vector<ConfigCache>
Hierarchy::readConfigFile(string configFile)
{
	vector<ConfigCache> result;
	// Produce the ConfigCache to send to create the levels
	return result;
}

std::ostream&
operator<<(std::ostream& out, const Level& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

std::ostream&
operator<<(std::ostream& out, const Hierarchy& obj)
{
    obj.print(out);
    out << std::flush;
    return out;
}

