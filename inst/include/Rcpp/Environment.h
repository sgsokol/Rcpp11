#ifndef Rcpp_Environment_h
#define Rcpp_Environment_h

namespace Rcpp{ 

    RCPP_API_CLASS(Environment_Impl), 
        public BindingPolicy< Environment_Impl<Storage> >
    {
        RCPP_API_IMPL(Environment_Impl)
        
    private:
        
        inline SEXP as_environment(SEXP x){  
            if( Rf_isEnvironment(x) ) return x ;
            SEXP asEnvironmentSym = Rf_install("as.environment");
            try {
                Shield<SEXP> res = Rcpp_eval( Rf_lang2( asEnvironmentSym, x ) );
                return res ;
            } catch( const eval_error& ex){
                throw not_compatible( "cannot convert to environment"  ) ; 
            }
        }
    
    public:
        
        inline void set(SEXP x){
            data = as_environment(x) ;        
        }
        
        Environment_Impl() : data(R_GlobalEnv){}
        
        Environment_Impl( const std::string& name ) {
            /* similar to matchEnvir@envir.c */
            if( name == ".GlobalEnv" ) {
                data = R_GlobalEnv ;
            } else if( name == "package:base" ){
                data = R_BaseEnv ;
            } else{
                SEXP res = R_NilValue ;
                try{
                    SEXP asEnvironmentSym = Rf_install("as.environment"); 
                    res = Rcpp_eval(Rf_lang2( asEnvironmentSym, Rf_mkString(name.c_str()) ) ) ;
                } catch( const eval_error& ex){
                    throw no_such_env(name) ;
                }
                data = res ;
            }
        }
        
        Environment_Impl( int pos ) {
            try{
                SEXP asEnvironmentSym = Rf_install("as.environment"); 
                data = Rcpp_eval( Rf_lang2( asEnvironmentSym, Rf_ScalarInteger(pos) ) ) ;
            } catch( const eval_error& ex){
                throw no_such_env(pos) ;
            }    
        }
    
        /**
         * The list of objects in the environment
         * 
         * the same as calling this from R: 
         * > ls( envir = this, all = all )
         *
         * @param all same meaning as in ?ls
         */ 
        SEXP ls(Rboolean all) const{
            if( is_user_database() ){
                R_ObjectTable *tb = (R_ObjectTable*)R_ExternalPtrAddr(HASHTAB(data));
                return tb->objects(tb) ;
            } else{
                return R_lsInternal( data, all) ;
            }
            return R_NilValue ;
        
        }
        SEXP ls(bool all) const{
            return ls( all ? TRUE : FALSE );    
        }
    
        /**
         * Get an object from the environment
         *
         * @param name name of the object
         *
         * @return a SEXP (possibly R_NilValue)
         */
        SEXP get(const std::string& name) const {
            SEXP res = Rf_findVarInFrame( data, Symbol(name) ) ;
            
            if( res == R_UnboundValue ) return R_NilValue ;
            
            /* We need to evaluate if it is a promise */
            if( TYPEOF(res) == PROMSXP){
                res = Rf_eval( res, data ) ;
            }
            return res ;
        }
    
        /**
         * Get an object from the environment or one of its
         * parents
         *
         * @param name name of the object
         *
         */
        SEXP find( const std::string& name) const {
            SEXP res = Rf_findVar( Symbol(name), data ) ;
            
            if( res == R_UnboundValue ) throw binding_not_found(name) ;
            
            /* We need to evaluate if it is a promise */
            if( TYPEOF(res) == PROMSXP){
                res = Rf_eval( res, data ) ;
            }
            return res ;
        }
    
        /**
         * Indicates if an object called name exists in the 
         * environment
         *
         * @param name name of the object
         *
         * @return true if the object exists in the environment
         */
        bool exists( const std::string& name ) const {
            SEXP res = Rf_findVarInFrame( data, Symbol(name)  ) ;
            return res != R_UnboundValue ;
        }
    
        /**
         * Attempts to assign x to name in this environment
         *
         * @param name name of the object to assign
         * @param x object to assign
         *
         * @return true if the assign was successfull
         * see ?bindingIsLocked
         *
         * @throw binding_is_locked if the binding is locked
         */
        bool assign( const std::string& name, SEXP x ) const{
            if( exists( name) && bindingIsLocked(name) ) throw binding_is_locked(name) ;
            Rf_defineVar( Symbol(name), x, data );
            return true ;
        }
    
        /**
         * wrap and assign. If there is a wrap method taking an object 
         * of WRAPPABLE type, then it is wrapped and the corresponding SEXP
         * is assigned in the environment
         *
         * @param name name of the object to assign
         * @param x wrappable object. anything that has a wrap( WRAPPABLE ) is fine
         */
        template <typename WRAPPABLE>
        bool assign( const std::string& name, const WRAPPABLE& x) const {
            return assign( name, wrap( x ) ) ;    
        }
    
        /**
         * @return true if this environment is locked
         * see ?environmentIsLocked for details of what this means
         */
        bool isLocked() const {
            return R_EnvironmentIsLocked(data);
        }
    
        /**
         * remove an object from this environment
         */
        bool remove( const std::string& name ){
            if( exists(name) ){
                if( bindingIsLocked(name) ){
                    throw binding_is_locked(name) ;
                } else{
                    /* unless we want to copy all of do_remove, 
                       we have to go back to R to do this operation */
                    SEXP internalSym = Rf_install( ".Internal" );
                    SEXP removeSym = Rf_install( "remove" );
                    Shield<SEXP> call = Rf_lang2(
                        internalSym, 
                        Rf_lang4(removeSym, Rf_mkString(name.c_str()), data, R_FalseValue) 
                     );
                    Rf_eval( call, R_GlobalEnv ) ;
                }
            } else{
                throw no_such_binding(name) ;
            }      
            return true;
        
        }
    
        /**
         * locks this environment. See ?lockEnvironment
         *
         * @param bindings also lock the bindings of this environment ?
         */
        void lock(bool bindings = false){
            R_LockEnvironment( data, bindings ? TRUE: FALSE ) ;
        }
    
        /**
         * Locks the given binding in the environment. 
         * see ?bindingIsLocked
         *
         * @throw no_such_binding if there is no such binding in this environment
         */
        void lockBinding(const std::string& name){
            if( !exists( name) ) throw no_such_binding(name) ;
            R_LockBinding( Symbol(name), data ); 
        }
    
        /**
         * unlocks the given binding
         * see ?bindingIsLocked
         *
         * @throw no_such_binding if there is no such binding in this environment
         */
        void unlockBinding(const std::string& name){
            if( !exists( name) ) throw no_such_binding(name) ;
            R_unLockBinding( Symbol(name), data );
        }
    
        /**
         * @param name name of a potential binding
         *
         * @return true if the binding is locked in this environment
         * see ?bindingIsLocked
         *
         * @throw no_such_binding if there is no such binding in this environment
         */
        bool bindingIsLocked(const std::string& name) const {
            if( !exists( name) ) throw no_such_binding(name) ;
            return R_BindingIsLocked(Symbol(name), data) ;
        }
    
        /**
         *
         * @param name name of a binding
         * 
         * @return true if the binding is active in this environment
         * see ?bindingIsActive
         *
         * @throw no_such_binding if there is no such binding in this environment
         */
        bool bindingIsActive(const std::string& name) const {
            if( !exists( name) ) throw no_such_binding(name) ;
            return R_BindingIsActive(Symbol(name), data) ;
        }
    
        /** 
         * Indicates if this is a user defined database.
         */
        bool is_user_database() const{
            return OBJECT(data) && Rf_inherits(data, "UserDefinedDatabase") ;
        }
    
        /**
         * @return the global environment. See ?globalenv
         */
        static Environment_Impl global_env(){
            return Environment(R_GlobalEnv) ;   
        }
    
        /**
         * @return The empty environment. See ?emptyenv
         */
        static Environment_Impl empty_env(){
            return Environment(R_EmptyEnv) ;
        }
    
        /**
         * @return the base environment. See ?baseenv
         */
        static Environment_Impl base_env(){
            return Environment(R_BaseEnv) ;   
        }
    
        /**
         * @return the base namespace. See ?baseenv
         */
        static Environment_Impl base_namespace(){
            return Environment(R_BaseNamespace) ;   
        }
    
        /**
         * @param name the name of the package of which we want the namespace
         *
         * @return the namespace of the package
         *
         * @throw no_such_namespace 
         */
        static Environment_Impl namespace_env(const std::string& package) {
            SEXP env = R_NilValue ;
            try{
                SEXP getNamespaceSym = Rf_install("getNamespace");
                env = Rcpp_eval( Rf_lang2(getNamespaceSym, Rf_mkString(package.c_str()) ) ) ;
            } catch( const eval_error& ex){
                throw no_such_namespace( package  ) ; 
            }
            return Environment( env ) ;        
        }
    
        /**
         * The parent environment of this environment
         */
        Environment_Impl parent() const {
            return Environment( ENCLOS(data) ) ;
        }
    
        /**
         * creates a new environment whose this is the parent
         */
        Environment_Impl new_child(bool hashed) {
            SEXP newEnvSym = Rf_install("new.env");
            return Environment( Rcpp_eval(Rf_lang3( newEnvSym, Rf_ScalarLogical(hashed), data )) );    
        }
        
    };

} // namespace Rcpp

#endif
