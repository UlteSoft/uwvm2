// Portable checkpoint DATA has one standalone module owner.
// The debugger reexports that owner; runtime consumers do not
// import a foreign debugger partition or its controller aggregate.
export module uwvm2.uwvm.debugger:checkpoint_state_identity;
export import uwvm2.uwvm.debugger.checkpoint_state_identity;
