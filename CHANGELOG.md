# CHANGELOG

## Version 1.0.0

### Implemented Functionality
- Developed a client-server application for communication using both TCP and UDP protocols.
- Implemented both TCP and UDP variants of the chat client to provide flexibility in communication.

### Known Limitations
1. **Confirmation Message Before Reply**:  
    - The current implementation expects the client to send a confirmation message before the server replies.  
    - While this design makes sense in controlled scenarios, it may not align with real-world use cases where immediate replies are expected.  

2. **Potential Real-World Behavior Variations**:  
    - The system assumes a strict request-confirmation-reply flow.  
    - In practical applications, deviations from this flow could lead to unexpected behavior.

### Notes
- The application is functional under the designed workflow.
- Known limitations are documented for clarity and transparency.
- Future iterations may address these limitations to enhance usability and adaptability.  