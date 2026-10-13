// Portable checkpoint DATA has one standalone module owner.
// The debugger reexports that owner; runtime consumers do not
// import a foreign debugger partition or its controller aggregate.
export module uwvm2.uwvm.debugger:checkpoint_codec;
export import uwvm2.uwvm.debugger.checkpoint_codec;
