/*
 * prometheus_counter.h
 * Developer: Satyabrata Bharati.
 * (C) Copyright 2022 Francesco Montorsi
 *
 *  Description:
 Defines PrometheusCounter class which creates and maintains "prometheus::Family<prometheus::Counter>" and
 "prometheus::Counter" class reference for each KPI.
 One object of PrometheusCounter class gets created per KPI, during application startup.

 prometheus::Family<prometheus::Counter> :
 This class creates a new metric of type prometheus::Counter.
 Every metric is uniquely identified by its name and a set of key-value pairs, also known as labels.
 Prometheus's query language allows filtering and aggregation based on metric name and these labels.
 Each new set of labels adds a new dimensional data and is exposed in Prometheus as a time series.
 It is possible to filter the time series with Prometheus's query language by appending a set of labels to match in
 curly braces ({}).

 prometheus::Counter :
 This class represents the metric type counter whose value can only increase.

 * This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
//------------------------------------------------------------------------------

#ifdef PROMETHEUS_SUPPORT
#ifndef _PROMETHEUS_COUNTER_H_
#define _PROMETHEUS_COUNTER_H_

#include <map>
#include <string>

// prometheus
#include <prometheus/counter.h>
#include <prometheus/family.h>
#include <prometheus/registry.h>

#include "prometheus_kpi.h"

class PrometheusCounter : public PrometheusKpi {
public:
    PrometheusCounter(std::shared_ptr<prometheus::Registry> prometheus_registry, std::string kpi_name,
        std::string kpi_description, std::map<std::string, std::string>& labels);

    PrometheusCounter(const PrometheusCounter&) = delete; // Copy constructor
    PrometheusCounter(PrometheusCounter&&) = delete; // Move constructor
    PrometheusCounter& operator=(const PrometheusCounter&) = delete; // Copy assignment operator
    PrometheusCounter& operator=(PrometheusCounter&&) = delete; // Move assignment operator

    virtual ~PrometheusCounter() = default;

    virtual void set_kpi_value(double kpi_value);
    virtual void set_kpi_value(double kpi_value, std::map<std::string, std::string>& labels);

private:
    PrometheusCounter() = default;

private:
    // This is a reference inside the prometheus::Registry (m_prometheus_registry) whose lifetime must be longer than
    // PrometheusCounter class.
    prometheus::Family<prometheus::Counter>& m_prometheus_kpi_family;
    // prometheus::Counter reference
    prometheus::Counter& m_prometheus_kpi;
};
#endif
#endif
