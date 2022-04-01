/*  promethus.h
 *
 * This library aims to enable Metrics-Driven Development for C++ services.
 * It implements the Prometheus Data Model, a powerful abstraction on which to collect and expose metrics. 
 * This lets you define and expose internal metrics via an HTTP endpoint on your application’s instance.
 *
 * See https://jupp0r.github.io/prometheus-cpp for more detailed interface documentation.
 *
 *
*/

#include <prometheus/gauge.h>
#include <prometheus/labels.h>
#include <prometheus/family.h>
#include <prometheus/counter.h>
#include <prometheus/exposer.h>
#include <prometheus/registry.h>
#include <prometheus/detail/future_std.h>

#include <array>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>


class CMonitorPromethues
{
public:
	CMonitorPromethues()
	{
	}

	void init();
	void addKPI(const std::string& str, double value);

	void setExposePort(const std::string& str)
	{
		m_exposer = prometheus::detail::make_unique<prometheus::Exposer>(str);
	}

	void setLabels(const std::map<std::string, std::string>& labels)
	{
		m_labels_map = labels;
	}


private:
	std::map<std::string, std::string> m_labels_map;
	std::unique_ptr<prometheus::Exposer> m_exposer;
	std::shared_ptr<prometheus::Registry> m_prometheus_registry = std::make_shared<prometheus::Registry>();
};
