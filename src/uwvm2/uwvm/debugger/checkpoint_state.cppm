// State DATA has a standalone owner so the runtime can import it without
// importing the debugger/controller module and creating a runtime cycle.
export module uwvm2.uwvm.debugger:checkpoint_state;
export import uwvm2.uwvm.debugger.checkpoint_state;
