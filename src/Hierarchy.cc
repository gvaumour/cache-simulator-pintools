#include <iostream>
#include <string>
#include <vector>

#include "Hierarchy.hh"
#include "HybridCache.hh"
#include "common.hh"

using namespace std;


Level::Level(){
	m_caches.clear();
	m_level = -1;
	m_system = NULL;
}

Level::Level(int level, std::vector<ConfigCache> configs, Hierarchy* system) : m_level(level), m_system(system){
	assert(configs.size()  >  0);
	m_caches.resize(configs.size());
	
	for(unsigned int i = 0 ; i < configs.size() ;  i++)
	{
		m_caches[i] = new HybridCache(configs[i].m_size, configs[i].m_assoc , \
						configs[i].m_blocksize, configs[i].m_nbNVMways, configs[i].m_policy, this);		
	} 
}

Level::~Level()
{
	for(auto p : m_caches)
		delete p;
}

void
Level::handleAccess(Access element)
{
	if(element.isInstFetch() && m_caches.size() == 2)
		return m_caches[1]->handleAccess(element);	
	else
		return m_caches[0]->handleAccess(element);		
}

bool
Level::lookup(Access element)
{
	if(element.isInstFetch() && m_caches.size() == 2)
		return m_caches[1]->lookup(element);
	else 
		return m_caches[0]->lookup(element);	
}

void
Level::deallocate(uint64_t addr)
{
	for(auto p : m_caches)
		p->deallocate(addr);
}

void
Level::signalDeallocate(uint64_t addr)
{
	if(m_level > 0)
		m_system->deallocateFromLevel(addr , m_level);
}

void 
Level::signalWB(uint64_t addr, bool isDirty)
{
	m_system->signalWB(addr,isDirty, m_level);
}
void 
Level::handleWB(uint64_t addr, bool isDirty)
{
	for(auto p : m_caches)
		p->handleWB(addr,isDirty);
}


void
Level::print(std::ostream& out) const
{
	out << "************************************************" << endl;
	out << "Level n°" << m_level << endl;
	for( unsigned i = 0 ; i < m_caches.size(); i++)
	{
		out << *(m_caches[i]);
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



Hierarchy::Hierarchy()
{
	ConfigCache L1Dataconfig (32768, 2 , 64 , "LRU", 0);
	vector<ConfigCache> firstLevel;
	firstLevel.push_back(L1Dataconfig);

	ConfigCache L1Instconfig = L1Dataconfig;
	firstLevel.push_back(L1Instconfig);

	ConfigCache L2config (262144, 8 , 64 , "InstructionPredictor", 4);
	vector<ConfigCache> secondLevel;
	secondLevel.push_back(L2config);
		
	m_nbLevel = 2;
	m_levels.resize(m_nbLevel);
	m_levels[0] = new Level(0 , firstLevel, this);
	m_levels[1] = new Level(1 , secondLevel, this);

}


Hierarchy::~Hierarchy()
{
	for(auto p : m_levels)
		delete p;
}

void
Hierarchy::print(std::ostream& out) const
{
	out << "ConfigCache : " << m_configFile << endl;
	out << "NbLevel : " << m_nbLevel << endl;
	for(unsigned i = 0 ; i < m_nbLevel ; i++)
	{
		out << "***************" << endl;
		m_levels[i]->print(out);
	}
	out << "***************" << endl;
}

void
Hierarchy::signalWB(uint64_t addr, bool isDirty, unsigned fromLevel)
{
	if(fromLevel < (m_levels.size()-1) ){
		
		// Forward WB request to the next higher level			
		m_levels[fromLevel+1]->handleWB(addr, isDirty);
	}	
}

void
Hierarchy::handleAccess(Access element)
{
	unsigned i= 0;
	bool hasData = false;

	//While the data is not found in the current level, transmit the request to the next level
	do
	{
		hasData = m_levels[i]->lookup(element);
		i++;
	}while(!hasData && i < m_nbLevel);
	
	i--;
	DPRINTF("HIERARCHY:: New Access : Data %#lx Req %s\n", element.m_address , memCmd_str[element.m_type]);
	if(hasData)
		DPRINTF("HIERARCHY:: Data found in level %d\n",i);
	else
		DPRINTF("HIERARCHY:: Data found in Main Memory\n");
		
	for(int a = i ; a >= 0 ; a--)
	{
		m_levels[a]->handleAccess(element);
		//DPRINTF("HIERARCHY:: Handled data in level %d\n",a);
	}
}


void
Hierarchy::deallocateFromLevel(uint64_t addr , unsigned level)
{
	//DPRINTF("Hierarchy::deallocateFromLevel %#lx, level : %d\n" , addr, level);
	
	int i = level-1;
	while(i >= 0)
	{
		m_levels[i]->deallocate(addr);
		i--;
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

