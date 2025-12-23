/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Author: Adrian Pietrzak
 * GitHub: https://github.com/AdrianPietrzak1998
 * Created: Jun 06, 2025
 */

#include "err_core.h"
#include "assert.h"

#if EC_TICK_FROM_FUNC

EC_TIME_t (*EC_get_tick)(void) = NULL;

#define EC_GET_TICK ((EC_get_tick != NULL) ? EC_get_tick() : ((EC_TIME_t)0))

void EC_tick_function_register(EC_TIME_t (*Function)(void))
{
    assert(Function != NULL);

    EC_get_tick = Function;
}

#else

EC_TIME_t *EC_tick = NULL;

#define EC_GET_TICK (*(EC_tick))

void EC_tick_variable_register(EC_TIME_t *Variable)
{
    assert(Variable != NULL);

    EC_tick = Variable;
}

#endif

/**
 * Initializes the error control instance.
 */
void EC_init(EC_instance_t *Instance, const EC_error_t *Errors, EC_runtimeData_t *RuntimeDataPtr, uint8_t NumberOfErrors)
{
    assert(Instance != NULL);
    assert(Errors != NULL);
    assert(RuntimeDataPtr != NULL);
    assert(NumberOfErrors > 0);
    assert(NumberOfErrors <= 64);

    Instance->Errors = Errors;
    Instance->NumberOfErrors = NumberOfErrors;
    Instance->RuntimeData = RuntimeDataPtr;
}

/**
 * Periodically checks and registers errors.
 */
void EC_poll(EC_instance_t *Instance)
{
    assert(Instance != NULL);

    uint64_t error;
    for (uint8_t i = 0; i < Instance->NumberOfErrors; i++)
    {
        if (!(Instance->ErrorReg & ((uint64_t)1 << i)) && (NULL != Instance->Errors[i].ErrFunc) && (0 == Instance->RuntimeData[i].WarningPending))
        {
            error = (Instance->Errors[i].ErrFunc(Instance->Errors[i].HelperNumber));
            if (0 == error)
            {
            	Instance->RuntimeData[i].LastNoErr = EC_GET_TICK;
            }
            else if (EC_GET_TICK - Instance->RuntimeData[i].LastNoErr >= Instance->Errors[i].TimeToErrorRegister)
            {
            	Instance->RuntimeData[i].WarningCnt++;
            	if (Instance->RuntimeData[i].WarningCnt >= Instance->Errors[i].WarningsToError)
            	{
            		Instance->ErrorReg |= error << i;
            		Instance->WarningReg &= ~((uint64_t)1 << i);
            		Instance->RuntimeData[i].WarningCnt = 0;
            	}
            	else
            	{
            		Instance->WarningReg |= ((uint64_t)1 << i);
            		Instance->RuntimeData[i].WarningPending = 1;
            	}
                Instance->RuntimeData[i].LastReg = EC_GET_TICK;
            }
        }
        if (EC_GET_TICK - Instance->RuntimeData[i].LastReg >= Instance->Errors[i].TimeToResetWarning)
        {
        	Instance->WarningReg &= ~((uint64_t)1 << i);
        	Instance->RuntimeData[i].WarningCnt = 0;
        	Instance->RuntimeData[i].WarningPending = 0;
        }
    }
}

/**
 * Returns the current 64-bit error register.
 */
uint64_t EC_getErrors(EC_instance_t *Instance)
{
    assert(Instance != NULL);

    return Instance->ErrorReg;
}

/**
 * Returns the state of the specified error.
 */
EC_err_state_t EC_getOneError(EC_instance_t *Instance, uint8_t ErrorNumber)
{
    assert(Instance != NULL);
    assert(ErrorNumber <= 64);

    uint64_t mask = (uint64_t)1 << ErrorNumber;

    return (Instance->ErrorReg & mask) ? EC_ERR : EC_NERR;
}

/**
 * Immediately checks and registers the specified error.
 */
EC_err_state_t EC_checkError(EC_instance_t *Instance, uint8_t ErrorNumber)
{
    assert(Instance != NULL);
    assert(ErrorNumber <= 64);

    if (Instance->ErrorReg & (1 << ErrorNumber))
    {
        return EC_ERR;
    }

    if (NULL != Instance->Errors[ErrorNumber].ErrFunc)
    {
    	EC_err_state_t error = (Instance->Errors[ErrorNumber].ErrFunc(Instance->Errors[ErrorNumber].HelperNumber));

        Instance->ErrorReg |= (uint64_t)error << ErrorNumber;

        return error;
    }

    return EC_NERR;
}

/**
 * Clears all error flags in the register.
 */
void EC_clearErr(EC_instance_t *Instance)
{
    assert(Instance != NULL);

    for (uint8_t i = 0; i < Instance->NumberOfErrors; i++)
    {
    	Instance->RuntimeData[i].LastNoErr = EC_GET_TICK;
    	Instance->RuntimeData[i].WarningPending = 0;
    }

    Instance->ErrorReg = 0;
}
