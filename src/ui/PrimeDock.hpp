#pragma once
#include "../core/ISwitcher.hpp"
#include "../core/PrimeScheduler.hpp"
#include <QTimer>
#include <QWidget>
#include <map>
#include <string>

class QTableWidget;
class QPushButton;
class QLabel;
class QDoubleSpinBox;
typedef struct obs_data obs_data_t;

class PrimeDock : public QWidget {
public:
	explicit PrimeDock(prime::ISwitcher *sw, QWidget *parent = nullptr);

	void startSwitching();
	void stopSwitching(const QString &reason = QString());
	void toggle();
	bool running() const { return sched_.state() == prime::PrimeScheduler::State::Running; }

	// A scene change we did not make: behaves exactly like Stop (full reset).
	void onManualSwitch();
	void refreshScenes();

	void load(obs_data_t *data);
	void save(obs_data_t *data);

private:
	struct Cfg { bool enabled = true; int period = 11; };

	void rebuildTable();
	void collect();            // widgets -> cfg_
	void setEditable(bool on);
	void arm();
	void onWake();
	void updateUi();

	prime::ISwitcher *sw_;
	prime::PrimeScheduler sched_;
	std::map<std::string, Cfg> cfg_;
	std::vector<std::string> rows_;

	QTableWidget *table_;
	QPushButton *button_;
	QLabel *status_;
	QDoubleSpinBox *dwell_;
	QTimer wake_;
	QTimer ui_;
};
