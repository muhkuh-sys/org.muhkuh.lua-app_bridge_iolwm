local class = require 'pl.class'
local AppBridgeModuleIolwm = class()

function AppBridgeModuleIolwm:_init(tAppBridge, tLog)
  self.pl = require'pl.import_into'()
  self.bit = require 'bit'

  local vstruct = require 'vstruct'                                                                                                                                                                        
  self.vstruct = vstruct                                                                                                                                                                                   
  tLog.debug('Using vstruct V%s', vstruct._VERSION)  

  self.tAppBridge = tAppBridge
  self.tLog = tLog

  -- This is the path to the module binary.
  self.strModulePath = 'netx/netx90_app_bridge_module_iolwm.bin'

  self.IOLWM_PARAM_VS_STACKREADY_IND_FORMAT = [[<
    bNumTracks:u1
    bIsMcuPresent:u1
    usStackRevisionBuild:u2
    bStackRevisionMinor:u1
    bStackRevisionMajor:u1
    usRfRevisionBuild:u2
    bRfRevisionMinor:u1
    bRfRevisionMajor:u1
  ]]
  self.IOLWM_PARAM_VS_STACKREADY_IND_T = vstruct.compile(self.IOLWM_PARAM_VS_STACKREADY_IND_FORMAT)
  self.IOLWM_PARAM_VS_STACKREADY_IND_SIZE = 10

  -- TODO: Get this from the binary.
  self.ulModuleLoadAddress = 0x000B8000
  self.ulModuleExecAddress = 0x000B8001
  self.ulModuleBufferArea = 0x000BC000

  self.IOLWM_COMMAND_WaitForPowerup = ${IOLWM_COMMAND_WaitForPowerup}
  self.IOLWM_COMMAND_ActivateSmiMode = ${IOLWM_COMMAND_ActivateSmiMode}
end


function AppBridgeModuleIolwm:initialize()
  local tAppBridge = self.tAppBridge
  local tLog = self.tLog

  -- Download the IOLWM module.
  local tModuleData, strError = self.pl.utils.readfile(self.strModulePath, true)
  if tModuleData==nil then
    tLog.error('Failed to load the module from "%s": %s', self.strModulePath, strError)
    error('Failed to load module.')
  end
  tAppBridge:write_area(self.ulModuleLoadAddress, tModuleData)

  -- TODO: Reset the module here?

  -- Initialize the UART connection to the IOLWM.
  tLog.info('Wait for IOLWM power up.')
  local ulValue = tAppBridge:call(self.ulModuleExecAddress, self.IOLWM_COMMAND_WaitForPowerup, self.ulModuleBufferArea)
  if ulValue~=0 then
    tLog.error('Failed to initialize the IOLWM module: 0x%08x', ulValue)
    error('Failed to initialize the IOLWM module.')
  else
    -- Read the buffer and extract the data.
    local strData = tAppBridge:read_area(self.ulModuleBufferArea, self.IOLWM_PARAM_VS_STACKREADY_IND_SIZE)
    -- Parse the data block.
    local tStackReadyIndication = self.IOLWM_PARAM_VS_STACKREADY_IND_T:read(strData)
    self.pl.pretty.dump(tStackReadyIndication)
  end
end


function AppBridgeModuleIolwm:activateSmiMode()
  local tAppBridge = self.tAppBridge
  local tLog = self.tLog

  local ulValue = tAppBridge:call(self.ulModuleExecAddress, self.IOLWM_COMMAND_ActivateSmiMode)
  if ulValue~=0 then
    tLog.error('Failed to activate SMI mode: 0x%08x', ulValue)
    error('Failed to activate SMI mode.')
  else
    tLog.info('SMI mode activated.')
  end
end


return AppBridgeModuleIolwm
