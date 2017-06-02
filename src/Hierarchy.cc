#include <iostream>
#include <string>
#include <vector>

#include "Hierarchy.hh"
#include "HybridCache.hh"
#include "common.hh"

using namespace std;


Level::Level(){
	m_dcache = NULL;
	m_icache = NULL;
	
	m_level = -1;
	m_system = NULL;
	m_isUnified = true;
}

Level::Level(int level, std::vector<ConfigCache> configs, Hierarchy* system) : m_level(level), m_system(system){
	assert(configs.size()  >  0);
	
	m_isUnified = true;
	m_printStats = configs[0].m_printStats;
	
	m_dcache = new HybridCache(configs[0].m_size, configs[0].m_assoc , \
					configs[0].m_blocksize, configs[0].m_nbNVMways, configs[0].m_policy, this);
	
	m_dcache->setPrintState(m_printStats);
	m_icache = NULL;		
	if(configs.size() == 2){
		m_isUnified = false;
		m_icache = new HybridCache(configs[1].m_size, configs[1].m_assoc , \
					configs[1].m_blocksize, configs[1].m_nbNVMways, configs[1].m_policy, this);
		m_icache->setPrintState(m_printStats);
	}
}

Level::~Level()
{
	delete m_dcache;
	if(!m_isUnified)
		delete m_icache;	
}

void
Level::handleAccess(Access element)
{
	if(element.isInstFetch() && !m_isUnified)
		return m_icache->handleAccess(element);	
	else
		return m_dcache->handleAccess(element);		
}

bool
Level::lookup(Access element)
{
	if(element.isInstFetch() && !m_isUnified)
		return m_icache->lookup(element);
	else 
		return m_dcache->lookup(element);	
}

void
Level::deallocate(uint64_t addr)
{
	m_dcache->deallocate(addr);
	if(!m_isUnified)
		m_icache->deallocate(addr);
}

void
Level::signalDeallocate(uint64_t addr)
{
	if(m_level > 0)
		m_system->deallocateFromLevel(addr , m_level);
}


void
Level::finishSimu()
{
	m_dcache->finishSimu();
	if(!m_isUnified)
		m_icache->finishSimu();
}

void 
Level::signalWB(uint64_t addr, bool isDirty)
{
	m_system->signalWB(addr,isDirty, m_level);
}
void 
Level::handleWB(uint64_t addr, bool isDirty)
{
	m_dcache->handleWB(addr,isDirty);
	if(!m_isUnified)
		m_icache->handleWB(addr,isDirty);
}

CacheEntry* 
Level::getEntry(uint64_t addr)
{
	CacheEntry* entry = m_dcache->getEntry(addr);
	
	if(!m_isUnified && entry == NULL)
		return m_icache->getEntry(addr);
	
	return entry;
}


void
Level::printResults(std::ostream& out)
{
	out << "************************************************" << endl;
//	out << "Level n°" << m_level << endl;
	if(m_isUnified)
	{
		out << "Unified Cache " << endl;
		m_dcache->printResults(out);
	}
	else
	{
		out << "Data Cache " << endl;
		m_dcache->printResults(out);
		out << "Instruction Cache " << endl;
		m_icache->printResults(out);
	}
}

void
Level::printConfig(std::ostream& out)
{
	out << "************************************************" << endl;
//	out << "Level n°" << m_level << endl;
	if(m_isUnified)
	{
		out << "Unified Cache " << endl;
		m_dcache->printConfig(out);
	}
	else
	{
		out << "Data Cache " << endl;
		m_dcache->printConfig(out);
		out << "Instruction Cache " << endl;
		m_icache->printConfig(out);
	}
}

void
Level::openNewTimeFrame()
{
	m_dcache->openNewTimeFrame();
	if(!m_isUnified)
		m_icache->openNewTimeFrame();
}


void
Level::print(std::ostream& out)
{
	printResults(out);
}


Hierarchy::Hierarchy()
{
	ConfigCache L1Dataconfig (32768, 2 , 64 , "LRU", 0);
	vector<ConfigCache> firstLevel;
	firstLevel.push_back(L1Dataconfig);

	ConfigCache L1Instconfig = L1Dataconfig;
	firstLevel.push_back(L1Instconfig);

	ConfigCache L2config ( TWO_MB , 16 , 64 , "RAP", 12);
	L2config.m_printStats = false;
	vector<ConfigCache> secondLevelConfig;
	secondLevelConfig.push_back(L2config);
	
		
	m_nbLevel = 2;
	m_nbCores = 1;
	m_levels.resize(m_nbLevel);
		
	stats_beginTimeFrame = 0;
	
	for(unsigned i = 0 ; i < m_nbCores ; i++){
		m_levels[0].push_back(new Level(0 , firstLevel, this) );
	}

	m_levels[1].push_back(new Level(1, secondLevelConfig , this));
	
}


Hierarchy::~Hierarchy()
{
	for(auto p : m_levels)
		for(auto a : p)
			delete a;
}


void
Hierarchy::printResults(ostream& out)
{
	for(unsigned i = 0 ; i < m_nbLevel ; i++)
	{
		out << "***************" << endl;
		for(unsigned j = 0 ; j < m_levels[i].size() ; j++)
		{
			out << "Core n°" << j << endl;		
			m_levels[i][j]->printResults(out);
		}
	}
	out << "***************" << endl;
}

void
Hierarchy::printConfig(ostream& out)
{
	out << "ConfigCache : " << m_configFile << endl;
	out << "NbLevel : " << m_nbLevel << endl;
	out << "NbCore : " << m_nbCores << endl;
	
	for(unsigned i = 0 ; i < m_nbLevel ; i++)
	{
		out << "Level n°" << i << endl;
		for(unsigned j = 0 ; j < m_levels[i].size() ; j++)
		{
			out << "Core n°" << j << endl;		
			m_levels[i][j]->printConfig(out);
		}
	}
	out << "***************" << endl;
}

void
Hierarchy::print(std::ostream& out) 
{
	printResults(out);
}

void
Hierarchy::signalWB(uint64_t addr, bool isDirty, unsigned fromLevel)
{
	if(fromLevel < (m_levels.size()-1) ){
		
		// Forward WB request to the next higher level	
		for(unsigned i = 0 ; i < m_levels[fromLevel+1].size() ; i++)
		{
			m_levels[fromLevel+1][i]->handleWB(addr, isDirty);		
		}		
	}	
}

void 
Hierarchy::finishSimu()
{
	for(unsigned i = 0 ; i < m_nbLevel ; i++)
	{
		for(unsigned j = 0 ; j < m_levels[i].size() ; j++)
		{
			m_levels[i][j]->finishSimu();
		}
	}
}


void
Hierarchy::handleAccess(Access element)
{
	unsigned level= 0 ,  core = 0;
	bool hasData = false;
	unsigned id_thread = element.m_idthread;
	
	start_debug = 1;	
	/*
	if(id_thread > 1)
		start_debug = 1;	
	*/

	DPRINTF("HIERARCHY:: New Access : Data %#lx Req %s Core %d\n", element.m_address , memCmd_str[element.m_type] , id_thread);
	//While the data is not found in the current level, transmit the request to the next level
	do
	{
		
		if(m_levels[level].size() == 1){
			core = 0;		
			hasData = m_levels[level][core]->lookup(element);
		}
		else{
			for(core = 0 ; core < m_levels[level].size() ; core++){
				hasData = m_levels[level][core]->lookup(element);
				if(hasData)
					break;
			}
		}
		
		level++;
		
	}while(!hasData && level < m_nbLevel);
	
	level--;
	
	if(hasData){
		DPRINTF("HIERARCHY:: Data found in level %d, Core %d\n",level , core);	
	}
	else{
		DPRINTF("HIERARCHY:: Data found in Main Memory , Core %d\n" , core);		
	}
	
	// If the cache line is allocated in another private cache  
	if(hasData && core != id_thread && level < (m_nbLevel-1)  && m_nbCores > 1) 
	{
		if(id_thread > m_nbCores)
		{
			assert(FALSE && "More threads than cores, not sure how the mapping threads/cores is done\n");
		}
			

		// If the cache block is an Instructionn no need to invalidate, can be shared 
		if(!element.isInstFetch())
		{
			DPRINTF("HIERARCHY:: Coherence Invalidation Level %d, Core %d\n",level , core);	
	
			CacheEntry* current = m_levels[level][core]->getEntry(element.m_address);
			assert(current != NULL && current->isValid);

			//We write back the data 
			signalWB(current->address, current->isDirty, level);
			deallocateFromLevel(current->address, level);
		
		}

		core = id_thread;
	}
	
	for(int a = level ; a >= 0 ; a--)
	{

		if(m_levels[a].size() == 1){		
			DPRINTF("HIERARCHY:: Handled data in level %d Core 0\n", a );
			m_levels[a][0]->handleAccess(element);
		}
		else{
			DPRINTF("HIERARCHY:: Handled data in level %d Core %d\n", a , core );		
			m_levels[a][core]->handleAccess(element);
		}
	}
	
	
	if( (cpt_time - stats_beginTimeFrame) > PREDICTOR_TIME_FRAME)
		openNewTimeFrame();
	
	DPRINTF("HIERARCHY:: End of handleAccess\n");
}


void
Hierarchy::openNewTimeFrame()
{
		for(unsigned i = 0 ; i < m_levels.size() ; i++)
		{
			for(unsigned j = 0 ; j < m_levels[i].size() ; j++)
			{
				m_levels[i][j]->openNewTimeFrame();
			}
		}
		
		stats_beginTimeFrame = cpt_time;
}

void
Hierarchy::deallocateFromLevel(uint64_t addr , unsigned level)
{
	DPRINTF("Hierarchy::deallocateFromLevel %#lx, level : %d\n" , addr, level);
	
	int i = level-1;
	while(i >= 0)
	{
		for(unsigned a = 0 ; a < m_levels[i].size() ; a++)
			m_levels[i][a]->deallocate(addr);
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

//std::ostream&
//operator<<(std::ostream& out, const Level& obj)
//{
//    obj.print(out);
//    out << std::flush;
//    return out;
//}

//std::ostream&
//operator<<(std::ostream& out, const Hierarchy& obj)
//{
//    obj.print(out);
//    out << std::flush;
//    return out;
//}

