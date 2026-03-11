#include "chipbase.h"
#include "printer.h"
#include "configfile.h"
#include "mcu.h"

ChipBase::ChipBase(std::shared_ptr<ConfigWrapper> config)
{
    printer = config->get_printer();
}

ChipBase::~ChipBase()
{
}

std::shared_ptr<MCU_pins> ChipBase::setup_pin(const std::string &pin_type, std::shared_ptr<PinParams> pin_params)
{
    return nullptr;
}

MCU_pins::MCU_pins(std::shared_ptr<ChipBase> mcu, std::shared_ptr<PinParams> pin_params) : mcu(mcu)
{
    pin = *(pin_params->pin);
}

MCU_pins::~MCU_pins()
{
}
