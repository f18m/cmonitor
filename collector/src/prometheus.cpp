/* 
 * prometheus.cpp
 *
*/

#include "prometheus.h"

void CMonitorPromethues::init()
{
    m_bPrometheusEnabled = true;
    m_exposer->RegisterCollectable(m_prometheus_registry);
}

bool CMonitorPromethues::is_prometheus_enabled()
{
   return m_bPrometheusEnabled;
}

void CMonitorPromethues::set_expose_port(const std::string& str)
   {
      m_exposer = prometheus::detail::make_unique<prometheus::Exposer>(str);
   }

void CMonitorPromethues::set_input_labels(const std::map<std::string, std::string>& labels)
   {
      m_labels_map = labels;
   }

void CMonitorPromethues::add_kpi(const std::string& str, double value,const std::string& s1,const std::string& s2)
{
    auto& metrics2 = prometheus::BuildGauge()
					  .Name(str)
					  .Help(str)
					  .Labels(m_labels_map)
					  .Register(*m_prometheus_registry)
					  .Add({{s1,s2}});
    metrics2.Set(value);
}
